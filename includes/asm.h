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

template <typename T>
class HashVec {
	struct mapped_type: public T{
		const std::string key;
		const int index;
		mapped_type(std::string key, int index, const T& value) : T(value), key(key), index(index)  {}
		mapped_type& operator=(const T& value) {
			T::operator=(value);
			return *this;
		}
	};
	std::unordered_map<std::string, int> map;
	std::vector<mapped_type> vec;
public:
	void put(const std::string& key, const T& value) {
		vec.push_back(mapped_type(key, vec.size(), value));
		map[key] = vec.size() - 1;
	}

	bool has(const std::string& key) {
		return map.count(key);
	}
	mapped_type& operator[](unsigned int index) {
		return vec[index];
	}
	mapped_type& operator[](const std::string& key) {
		if (!map.count(key))
			put(key, T{});

		return vec[map[key]];
	}
};


namespace ASM {
	using string = std::string;
	using uint = unsigned int;
	template <typename T> using vector = std::vector<T>;

	/*struct ELF_Symbol {
		int name;
		int value;
		int size;
		char type : 4,
			binding : 4;
		char reserved;
		char section;
	};*/
	struct Symbol {
		string section;
		uint offset;
		bool isLocal = true;
	};
	struct Section {
		uint counter = 0;
		std::vector<uint8_t> bytes;
	};

	typedef enum {

	} reloc_t;

	struct Relocation {
		string section;
		uint offset;
		reloc_t type;
		uint num;
	};

	HashVec<Symbol> symtable;
	HashVec<Section> sections;
	HashVec<Relocation> relocations;
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
			std::cout << iter->section << ":\t";
			for (auto& datum : iter->data) {
				//print parsed line on string
				for (auto& value : datum.values)
					std::cout << value << " || ";

				if (datum.flags & SECTION) {
					const std::string& section_name = datum.values[0];
					// create section entry if it doesn't exist
					if (!sections.has(section_name))
						sections.put(section_name, Section{});
					// add symbol entry to symtable
					if (symtable.has(section_name))
						throw symbol_redeclaration(iter->line_num, iter->line);
					symtable[section_name] = Symbol {section_name, sections[iter->section].counter };
				}
				else if (datum.flags & LABEL) {
					if (symtable.has(datum.values[0]))
						throw symbol_redeclaration(iter->line_num, iter->line);
					symtable[datum.values[0]] = Symbol{ iter->section, sections[iter->section].counter };
				}
				else if (datum.flags & INSTRUCTION) {
					int count = 0;
					for (int i = 1; (i <= OP_NUM) && (datum.flags & ENABLE(i)); i++)
						count++;

					int bytes = 1 + count * (datum.flags & EXTENDED ? WORD_SZ*2 : WORD_SZ);
					sections[iter->section].counter += bytes;
				}
				std::cout << sections[iter->section].counter;

			}
			std::cout << '\n';
		}

		//second pass
		for (source_iterator iter{ input_path }; iter != EOF; ++iter) {

		}
	}


}

#endif