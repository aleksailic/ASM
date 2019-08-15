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


namespace ASM {
	using string = std::string;
	using uint = unsigned int;
	template <typename T> using vector = std::vector<T>;

	namespace utils {
		inline int bitsize(unsigned int num) {
			int count = 0;
			while (num) {
				num >>= 1;
				count++;
			}
			return count;
		}
		// converts string to integer with addition that if string contains a single char it will convert accordingly
		int sctoi(const string& str) {
			try {
				return std::stoi(str);
			}
			catch (std::invalid_argument& exception) {
				if (str.length() == 1 && std::isalpha(str[0]))
					return str[0];
				else throw exception;
			}
		}
	}

	namespace streams {
		auto& warning = std::cerr;
	}

	struct Symbol {
		string section;
		uint offset;
		bool isLocal = true;
	};
	struct Section {
		uint counter = 0;
	private:
		vector<uint8_t> data;
		class stream {
			Section& m_section;
			uint bitsize;
		public:
			stream(Section& section, uint bitsize) :m_section(section), bitsize(bitsize) {}
			const stream& operator<<(int number) const {
				if (utils::bitsize(number) > bitsize)
					throw std::overflow_error("Overflow. Number passed is larger than stream");

				for (int i = bitsize - 8; i >= 0; i -= 8) {
					m_section.counter++;
					m_section.data.push_back( number & (0xFF << i) );
				}
				return *this;
			}
		};
	public:
		string memdump() {
			std::ostringstream oss;
			for (auto& byte : data)
				oss << std::hex << byte;
			return oss.str();
		}
		const stream bytes{ *this, 8 };
		const stream words { *this, WORD_SZ * 8};
		const stream dwords { *this, DWORD_SZ * 8 };
		
	};

	
	struct Relocation {
		typedef enum {
			R_386_PC16
		} reloc_t;

		string section;
		uint offset;
		uint num;
		reloc_t type;
	};

	struct Constant {
		int value;
	};

	enum {
		Z	= 1 << 0, //zero
		O	= 1 << 1, //overflow
		C	= 1 << 2, //carry
		N	= 1 << 3, //negative
		Tr	= 1 << 13, //timer
		Tl	= 1 << 14, //terminal
		I	= 1 << 15 //interrupt
	};

	struct Instruction {
		typedef uint16_t flags_t;
		flags_t flags = 0;
	};

	template <typename T>
	class HashVec {
		struct mapped_type : public T {
			const std::string key;
			const int index;
			mapped_type(std::string key, int index, const T& value) : T(value), key(key), index(index) {}
			mapped_type& operator=(const T& value) {
				T::operator=(value);
				return *this;
			}
		};
		struct init_type {
			const std::string key;
			T value;
		};
		std::unordered_map<std::string, int> map;
		std::vector<mapped_type> vec;
	public:
		HashVec() = default;
		HashVec(std::initializer_list<init_type> list) {
			for (auto& elem : list) {
				put(elem.key, elem.value);
			}
		}
		void put(const std::string& key, const T& value) {
			vec.push_back(mapped_type(key, vec.size(), value));
			map[key] = vec.size() - 1;
		}
		bool has(const std::string& key) {
			return map.count(key);
		}

		auto begin() {
			return vec.begin();
		}
		auto end() {
			return vec.end();
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

	template<>
	bool HashVec<Symbol>::has(const string& key) {
		return (!map.count(key) || vec[map[key]].section == "RELOC") ? false : true;
	}



	HashVec<Symbol> symtable;
	HashVec<Section> sections;
	HashVec<Relocation> relocations;
	HashVec<Constant> constants;
	HashVec<Instruction> optable = {
		{"halt"},
		{"xchg"},
		{"int"},
		{"mov", Z | N},
		{"add", Z | O | C | N},
		{"sub", Z | O | C | N},
		{"mul", Z | N},
		{"div", Z | N},
		{"cmp", Z | O | C | N},
		{"not", Z | N},
		{"and", Z | N},
		{"or", Z | N},
		{"xor", Z | N},
		{"test", Z | N},
		{"shl", Z | C | N},
		{"shr", Z | C | N},
		{"push"},
		{"pop"},
		{"jmp"},
		{"jeq"},
		{"jne"},
		{"jgt"},
		{"call"},
		{"ret"},
		{"iret"}
	};
	
	string input_path, output_path;

	void init(const string& input, const string& output) {
		input_path = input;
		output_path = output;
	}


	void assemble() {
		// first pass
		std::cout << "FIRST PASS!\n";
		for (source_iterator iter{ input_path }; (iter != EOF) || (iter->data[0].flags & END); ++iter) {
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
					symtable[section_name] = Symbol{ section_name, sections[iter->section].counter };
				}
				else if (datum.flags & LABEL) {
					if (symtable.has(datum.values[0]))
						throw symbol_redeclaration(iter->line_num, iter->line);
					symtable[datum.values[0]] = Symbol{ iter->section, sections[iter->section].counter };
				}
				else if (datum.flags & INSTRUCTION) {
					int bytes = INSTR_SZ; // inital value for opcode

					for (int i = 1; (i <= OP_NUM) && (datum.flags & ENABLE(i)); i++) {
						flags_t mode = MODE_MASK(datum.flags, i);
						if (mode == SYM(i))
							bytes += 1 + datum.flags & EXTENDED ? DWORD_SZ : WORD_SZ;
						else if (mode == SYMADDR(i))
							bytes += 1 + DWORD_SZ;
						else if (mode == IMMED(i))
							bytes += utils::bitsize(utils::sctoi(datum.values[i])) > WORD_SZ ? DWORD_SZ : WORD_SZ;
						else if (mode == REGIND16(i) || mode == REGIND8(i))
							bytes += utils::bitsize(utils::sctoi(datum.values[i])) > WORD_SZ ? DWORD_SZ : WORD_SZ;
						else if (mode == (REGIND16(i) | SYM(i)) || mode == (REGIND8(i) | SYM(i)))
							bytes += datum.flags & EXTENDED ? DWORD_SZ : WORD_SZ;
						else if (mode == MEM(i))
							bytes += 1 + DWORD_SZ;
					}

					sections[iter->section].counter += bytes;
				}
				else if (datum.flags & ALLOC) {
					int multiplier = datum.values[0] == "byte" ? WORD_SZ : DWORD_SZ;
					sections[iter->section].counter += (datum.values.size() - 1) * multiplier;
				}
				else if (datum.flags & ALIGN) {
					int num = std::stoi(datum.values[0]);
					if (!(((num & ~(num - 1)) == num) ? num : 0))
						throw syntax_error(iter->line_num, datum.values[0], "align number must be power 2");
					sections[iter->section].counter += sections[iter->section].counter % num;
				}
				else if (datum.flags & SKIP) {
					sections[iter->section].counter += std::stoi(datum.values[0]) * WORD_SZ;
				}
				else if (datum.flags & EQU) {
					constants[datum.values[0]].value = utils::sctoi(datum.values[1]);
				}
				std::cout << sections[iter->section].counter;

			}
			std::cout << '\n';
		}

		// restart section counters
		for (auto& section : sections)
			section.counter = 0;


		std::cout << "\nSECOND PASS!\n";
		//second pass
		for (source_iterator iter{ input_path }; iter != EOF; ++iter) {
			std::cout << iter->section << ":\t";
			for (auto& datum : iter->data) {
				//print parsed line on string
				for (auto& value : datum.values)
					std::cout << value << " || ";
				if (datum.flags & ALLOC) {
					auto& stream = datum.values[0] == "byte" ? sections[iter->section].words : sections[iter->section].dwords;
					for (auto it = ++datum.values.begin(); it != datum.values.end(); ++it) {
						stream << utils::sctoi(*it);
					}
				} else if (datum.flags & RELOC) {
					symtable[datum.values[1]].isLocal = false;
				} else if (datum.flags & INSTRUCTION) {
					if (!optable.has(datum.values[0]))
						throw syntax_error(iter->line_num, datum.values[0], "Instruction doesn't exist");
					uint8_t instr_desc = optable[datum.values[0]].index << 2;
					if (datum.flags & EXTENDED)
						instr_desc |= 0x4;

					sections[iter->section].bytes << instr_desc;

					if((datum.flags & ENABLE(2)) && ((datum.flags,1) == IMMED(1) || MODE_MASK(datum.flags, 1) == SYM(1)))
						throw syntax_error(iter->line_num, iter->line, "IMMED for dst not allowed.");

					for (int i = 1; (i <= OP_NUM) && (datum.flags & ENABLE(i)); i++) {
						flags_t mode = MODE_MASK(datum.flags, i);
						uint8_t op_desc = ADDR_MASK(datum.flags, i);


						if (mode == REGDIR(i))
							op_desc |= GET_REG(datum.values[i]) << 1;
						else if (mode == SYM(i) || mode == SYMADDR(i)) { // TODO: SYMADDR not done
							if (constants.has(datum.values[i]))
								sections[iter->section].dwords << constants[datum.values[i]].value;
							else if (symtable.has(datum.values[i]))
								sections[iter->section].dwords << symtable[datum.values[i]].offset;
							else { // not in symtable
								symtable[datum.values[i]] = Symbol{ "RELOC" };
								relocations[datum.values[i]] = Relocation{iter->section, sections[iter->section].counter, symtable[datum.values[i]].index};
								sections[iter->section].dwords << 0xFFFF; // TODO: parametrize this
							}
						} else if (mode == IMMED(i)) {
							int bytes = utils::bitsize(utils::sctoi(datum.values[i]));
						}							
					}
				}
			}
			std::cout << '\n';
		}
	}


}

#endif