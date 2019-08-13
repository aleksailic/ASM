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
}
TEST_CASE("HasVec structure check") {
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

}


int main(int argc, char* argv[]) {
	const char* args[] = { argv[0] };
	Catch::Session().run(1, args);

	ASM::init(argc, argv);

	ASM::assemble();
}