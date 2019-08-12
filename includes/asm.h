#ifndef __ASM_H__
#define __ASM_H__

#include "asm/parser.h"
#include "asm/source_iterator.h"

#include <iostream>
#include <string>
#include <algorithm>
#include <regex>
#include <unordered_map>
#include <fstream>

/*template <typename T>
class HashVec {
	unordered_map<std::string, std::ref<T>> hashmap;
	std::vector<T> data;
public:
	T& operator[](int n) {
		return data[n];
	}
	T& operator[](std::string key) {
		return hashmap[key];
	}
	void put(std::string key, const T& element) {
		data.push_back(element);
		hashmap[key] = data.back();
	}
};*/

namespace ASM {
	using string = std::string;
	template <typename T> using vector = std::vector<T>;

	struct Symbol {
		int name;
		int value;
		int size;
		char type : 4,
			binding : 4;
		char reserved;
		char section;
	};

	//HashVec<Symbol> symtable;
	string input_path, output_path;

	void init(int argc, char* argv[]) {
		try {
			cxxopts::Options options("Assembler", " -o output (default a.o) source");
			options.add_options()
				("o,output", "Output file", cxxopts::value<string>()->default_value("a.o"))
				("h,help", "Print help")
				("source", "Source file", cxxopts::value<string>());

			options.positional_help("<SOURCE>");
			options.parse_positional({ "source" });
			auto result = options.parse(argc, argv);

			auto arguments = result.arguments();
			if (result.count("help")) {
				std::cout << options.help({ "" }) << std::endl;
				exit(0);
			}

			input_path = result["source"].as<string>();
			output_path = result["output"].as<string>();
		}
		catch (std::exception& ex) {
			std::cerr << ex.what() << '\n';
			exit(1);
		}
	}

	void assemble() {
		// first pass
		for (source_iterator iter{ input_path }; iter != EOF; ++iter) {
			if (!iter->data.empty()) {
				std::cout << iter->section << ": ";
				for (auto& datum : iter->data) {
					std::cout << std::hex << "0x" << datum.flags << '\t';
					for (auto& values : datum.values)
						std::cout << values << " || ";
				}
				std::cout << '\n';
			}

		}

		//second pass
		for (source_iterator iter{ input_path }; iter != EOF; ++iter) {

		}
	}


}

#endif