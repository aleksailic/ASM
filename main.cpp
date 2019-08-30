#define CATCH_CONFIG_RUNNER
#include "cxxopts.hpp"
#include "catch.hpp"

#include "asm.h"

static const ASM::parser get_parser(ASM::flags_t type) {
	for (auto& parser : ASM::parsers) {
		if (parser.flags & type)
			return parser;
	}
	throw std::runtime_error("Fatal error! Parser not defined");
}

TEST_CASE("REGEX check") {
	using namespace ASM;

	SECTION("Checking if LABELS are correctly detected") {
		parsed_t data = get_parser(LABEL).parse("\t label1: \n");
		REQUIRE(data.values[0] == "label1");
	}

	SECTION("Checking if SECTIONS are correctly detected") {
		parsed_t data = get_parser(SECTION).parse("\t.section \".text\" \n");
		REQUIRE(data.values[0] == "text");
	}

	SECTION("Checking if LABEL with INSTRUCTION is properly decoded") {
		std::ofstream testfile;
		testfile.open("testfile", std::ios::out | std::ios::trunc);
		testfile << "label1: mov ax, bp";
		testfile.close();

		for (source_iterator iter("testfile"); iter != EOF; ++iter) {
			REQUIRE((iter->data[0].flags & LABEL) == LABEL);
			REQUIRE(iter->data[0].values[0] == "label1");

			REQUIRE((iter->data[1].flags & INSTRUCTION) == INSTRUCTION);
			REQUIRE(iter->data[1].values[0] == "mov");
			REQUIRE(iter->data[1].values[1] == "ax");
			REQUIRE(iter->data[1].values[2] == "bp");
		}

		std::remove("testfile");
	}

	SECTION("Checking if parser recursion works properly") {
		parsed_t data = get_parser(ALLOC).parse(".byte 1,2 ,3,4,  5, 6");
		REQUIRE(data.values[0] == "byte");
		REQUIRE(data.values[1] == "1");
		REQUIRE(data.values[2] == "2");
		REQUIRE(data.values[3] == "3");
		REQUIRE(data.values[4] == "4");
		REQUIRE(data.values[5] == "5");
		REQUIRE(data.values[6] == "6");
	}

	SECTION("Checking NUMCHAR parsing") {
		parsed_t data = get_parser(ALLOC).parse(".byte 'W', 'O', 'R', 'D', '\\n'");
		REQUIRE(data.values[1] == "W");
		REQUIRE(data.values[5] == "\\n");
	}

	SECTION("Checking skip/align") {
		parsed_t data = get_parser(SKIP).parse(".skip 5,7");
		REQUIRE(data.values[1] == "5");
		REQUIRE(data.values[2] == "7");
		data = get_parser(ALIGN).parse(".align 5,   7");
		REQUIRE(data.values[1] == "5");
		REQUIRE(data.values[2] == "7");
	}

	SECTION("Checkin SYMREL") {
		parsed_t data = get_parser(INSTRUCTION).parse("call $skip");
		REQUIRE(data.values[0] == "call");
		REQUIRE(data.values[1] == "skip");
	}

	SECTION("Checking addrmode regex") {
		parsed_t data = get_parser(INSTRUCTION).parse("mov [r7][test]");
		REQUIRE(MODE_MASK(data.flags, 1) == (REGIND16(1) | SYMABS(1)));

		data = get_parser(INSTRUCTION).parse("mov r7[test]");
		REQUIRE(MODE_MASK(data.flags, 1) == (REGIND16(1) | SYMABS(1)));

		data = get_parser(INSTRUCTION).parse("jne $printf");
		REQUIRE(MODE_MASK(data.flags, 1) == (IMMED(1) | SYMREL(1)));

		data = get_parser(INSTRUCTION).parse("movw ax, 3560");
		REQUIRE(MODE_MASK(data.flags, 1) == REGDIR(1));
		REQUIRE(MODE_MASK(data.flags, 2) == IMMED(2));
;	}
}

TEST_CASE("HasVec structure check") {
	using namespace ASM;
	HashVec<std::string> symtable;

	symtable["jovana"] = "jankovic";
	symtable[0] = "milankovic";

	REQUIRE(symtable["jovana"] == "milankovic");
	REQUIRE(symtable["jovana"].key == "jovana");
	REQUIRE(symtable["jovana"].index == 0);
	REQUIRE(symtable[0].index == 0);

	symtable["milenko"] = "milenkovic";

	REQUIRE(symtable[1].key == "milenko");
	REQUIRE(symtable["milenko"].index == 1);

	REQUIRE((optable["mov"].flags & E) == E);
	REQUIRE((optable["jne"].flags & E) == 0);
}

TEST_CASE("Address mask chekcs") {
	using namespace ASM;
	REQUIRE(MODE_MASK(REGDIR(1) | INSTRUCTION, 1) == REGDIR(1));
	REQUIRE(MODE_MASK(IMMED(2) | INSTRUCTION, 2) == IMMED(2));
	REQUIRE(MODE_MASK(IMMED(1) | INSTRUCTION, 1) != IMMED(2));

	REQUIRE(ADDR_MASK(REGDIR(2), 2) == 0x1 << 5);
	REQUIRE(ADDR_MASK(REGIND8(1), 1) == 0x3 << 5);
}

TEST_CASE("Stream test") {
	using namespace ASM;

	SECTION("Checking basic functionality") {
		Section test;

		REQUIRE(test.counter == 0);

		test.bytes << 0xAD;
		test.dwords << 0x0A0B;

		REQUIRE(test.counter == 3);
		REQUIRE(test.memdump() == "AD0B0A");
	}


	SECTION("Checking if works with HashVec") {
		sections.put("test", Section{});
		auto& test = sections["test"];

		REQUIRE(sections["test"].counter == 0);

		test.bytes << 0xAD;
		test.dwords << 0x0A0B;
		REQUIRE(test.counter == 3);
		//REQUIRE(test.memdump() == "AD0B0A");
	}

}

TEST_CASE("Running testfiles") {
	
}

using string = std::string;

int main(int argc, char* argv[]) {
	try {
		cxxopts::Options options(argv[0], "Assembler for a simple 16-bit 2-address processor with von Neumann architecture ");
		options.add_options()
			("o,output", "Output file", cxxopts::value<string>()->default_value("a.o"))
			("h,help", "Print help")
			("t,test", "Run tests")
			("source", "Source file", cxxopts::value<string>());

		options.positional_help("<SOURCE>");
		options.parse_positional({ "source" });
		auto result = options.parse(argc, argv);

		auto arguments = result.arguments();
		if (result.count("help")) {
			std::cout << options.help({ "" }) << std::endl;
			exit(0);
		}

		if (result.count("test")) {
			const char* args[] = { argv[0] };
			Catch::Session().run(1, args);
			exit(0);
		}

		ASM::init(result["source"].as<string>(), result["output"].as<string>());

		ASM::assemble();
	}
	catch (std::exception& ex) {
		std::cerr << ex.what() << '\n';
		exit(1);
	}
}