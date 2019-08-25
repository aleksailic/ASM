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

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN
#endif


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
		uint16_t sctoi(const string& str) {
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
#ifdef LITTLE_ENDIAN
				for (int i = 0; i < bitsize; i += 8) {
					m_section.counter++;
					m_section.data.push_back(number >> i);
				}
#else
				for (int i = bitsize - 8; i >= 0; i -= 8) {
					m_section.counter++;
					m_section.data.push_back(number >> i);
				}
#endif
				return *this;
			}
		};
	public:
		Section() = default;
		Section(const Section& rhs) {
			counter = rhs.counter;
			data = rhs.data;
		}
		string memdump() const{
			std::ostringstream oss;
			for (auto& byte : data)
				oss << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << (int)byte;
			return oss.str();
		}
		const stream bytes { *this, 8 };
		const stream words { *this, WORD_SZ * 8};
		const stream dwords { *this, DWORD_SZ * 8 };

		const stream& get_stream(int byte_number) {
			switch (byte_number) {
				case WORD_SZ: return words;
				case DWORD_SZ: return dwords;
				default: throw std::runtime_error("Illegal byte number passed");
			}
		}
		
	};

	
	struct Relocation {
		typedef enum {
			R_386_PC16, R_386_16
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
		Z	= 1 << 0,  //zero
		O	= 1 << 1,  //overflow
		C	= 1 << 2,  //carry
		N	= 1 << 3,  //negative
		E	= 1 << 4,  //extensible
		Tr	= 1 << 13, //timer
		Tl	= 1 << 14, //terminal
		I	= 1 << 15  //interrupt
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

		auto begin() const {
			return vec.begin();
		}
		auto end() const {
			return vec.end();
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

		template <typename U>
		friend std::ostream& operator<<(std::ostream& stream, const HashVec<U>& hashvec);
	};

	template<>
	bool HashVec<Symbol>::has(const string& key) {
		return (!map.count(key) || vec[map[key]].section == "RELOC") ? false : true;
	}

	// specialization for pretty hashvec output
	template<>
	std::ostream& operator<<(std::ostream& stream, const HashVec<Symbol>& symbols) {
		stream << "#tabela simbola\n";
		stream << "#ime" << '\t' << "sek" << '\t' << "vr." << '\t' << "vid." << '\t' << "r.b." << '\n';
		for (const auto& symbol : symbols) {
			stream << symbol.key << '\t' << symbol.section << '\t' << symbol.offset << '\t' << (symbol.isLocal ? "local" : "global") << '\t' << symbol.index << '\n';
		}
		return stream;
	}
	template<>
	std::ostream& operator<<(std::ostream& stream, const HashVec<Constant>& constants) {
		stream << "#tabela konstanti\n";
		stream << "#ime" << '\t' << "vr." << '\t' << "r.b." << '\n';
		for (const auto& constant : constants) {
			stream << constant.key << '\t' << constant.value << '\t' << constant.index << '\t' << '\n';
		}
		return stream;
	}
	template<>
	std::ostream& operator<<(std::ostream& stream, const HashVec<Relocation>& relocations) {
		std::unordered_map<std::string, std::vector<Relocation>> map;
		for (const auto& relocation : relocations)
			map[relocation.section].push_back(relocation);

		for (const auto& rel_section : map) {
			stream << "#.ret." << rel_section.first << '\n';
			stream << "#ofset" << '\t' << "tip" << "\t\t"  << "vr[." << rel_section.first << "]:\t" << '\n';
			for (const auto& relocation : rel_section.second) {
				stream << "0x" << std::setfill('0') << std::setw(DWORD_SZ * 2) << std::hex << std::uppercase;
				stream << relocation.offset << '\t' << (relocation.type == Relocation::reloc_t::R_386_16 ? "R_386_16" : "R_386_PC16") << '\t' << std::dec << relocation.num << '\n';
			}
		}
		
		return stream;
	}
	template<>
	std::ostream& operator<<(std::ostream& stream, const HashVec<Section>& sections) {
		for (const auto& section : sections) {
			stream << "#." << section.key << " (" << section.counter << ")\n";
			string memdump = section.memdump();
			for (int i = 0; i + 1 < memdump.length(); i += 2)
				stream << memdump[i] << memdump[i + 1] << " ";
			stream << '\n';
		}
		return stream;
	}

	HashVec<Symbol> symtable;
	HashVec<Section> sections;
	HashVec<Relocation> relocations;
	HashVec<Constant> constants;
	HashVec<Instruction> optable = {
		{"halt"},
		{"xchg", E},
		{"int"},
		{"mov", Z | N | E},
		{"add", Z | O | C | N | E},
		{"sub", Z | O | C | N | E},
		{"mul", Z | N | E},
		{"div", Z | N | E},
		{"cmp", Z | O | C | N | E},
		{"not", Z | N | E},
		{"and", Z | N | E},
		{"or", Z | N | E},
		{"xor", Z | N | E},
		{"test", Z | N | E},
		{"shl", Z | C | N | E},
		{"shr", Z | C | N | E},
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
					if (!optable.has(datum.values[0]))
						throw syntax_error(iter->line_num, datum.values[0], "Instruction doesn't exist");
					if (!(optable[datum.values[0]].flags & E) && (datum.flags & EXTENDED))
						throw syntax_error(iter->line_num, iter->line, "This instruction has fixed size");
					int bytes = INSTR_SZ; // inital value for opcode
					int op_sz = optable[datum.values[0]].flags & E ? (datum.flags & EXTENDED ? DWORD_SZ : WORD_SZ) : DWORD_SZ;

					for (int i = 1, j = 1; (i <= OP_NUM) && (datum.flags & ENABLE(i)); i++, j++) {
						bytes += 1; //op<num>_desc sz
						flags_t mode = MODE_MASK(datum.flags, i);

						// error checking for improper size
						if ((datum.flags & EXTENDED) && (datum.flags & REDUCED(i)))
							throw syntax_error(iter->line_num, iter->line, "You cannot use extended instruction with reduced register size");

						if (mode == IMMED(i) || mode == (IMMED(i) | SYMABS(i)))
							bytes += op_sz;
						else if (mode == (IMMED(i) | SYMREL(i)) || mode == (IMMED(i) | SYMADR(i)))
							bytes += DWORD_SZ;
						else if (mode == (REGIND(i) | REGIND16(i))) {
							int shift_sz = utils::bitsize(utils::sctoi(datum.values[i])) > WORD_SZ ? DWORD_SZ : WORD_SZ;
							if (shift_sz == WORD_SZ)
								SET_MODE(datum.flags, i, REGIND8(i));
							bytes += shift_sz;
						}
						else if (mode == (REGIND(i) | REGIND16(i) | SYMABS(i))) {
							j++;
							int shift_sz = constants.has(datum.values[j]) ? (utils::bitsize(utils::sctoi(datum.values[j])) > WORD_SZ ? DWORD_SZ : WORD_SZ) : DWORD_SZ;
							if (shift_sz == WORD_SZ)
								SET_MODE(datum.flags, i, REGIND8(i));
							bytes += shift_sz;
						}
						else if (mode == MEM(i))
							bytes += DWORD_SZ;
						else if ((mode == REGDIR(i) || mode == REGIND(i)) && datum.flags & REDUCED(i))
							j++;

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
					sections[iter->section].counter += std::stoi(datum.values[1]);
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
				} else if (datum.flags & SKIP) {
					for (int i = 0; i < std::stoi(datum.values[1]); i++)
						sections[iter->section].bytes << std::stoi(datum.values.size() > 2 ? datum.values[2] : "0");
				} else if (datum.flags & INSTRUCTION) {
					uint8_t instr_desc = optable[datum.values[0]].index << 3;
					if (datum.flags & EXTENDED)
						instr_desc |= 0x4;

					sections[iter->section].bytes << instr_desc; // pushing instruction desecriptor to stream

					if((datum.flags & ENABLE(2)) && (MODE_MASK(CLEAR_SYM(datum.flags),1) == IMMED(1)))
						throw syntax_error(iter->line_num, iter->line, "IMMED for dst not allowed.");

					int op_sz = optable[datum.values[0]].flags & E ? (datum.flags & EXTENDED ? DWORD_SZ : WORD_SZ) : DWORD_SZ;

					for (int i = 1, j = 1; (i <= OP_NUM) && (datum.flags & ENABLE(i)); i++, j++) {
						flags_t mode = MODE_MASK(datum.flags, i);
						uint8_t op_desc = ADDR_MASK(datum.flags, i);

						using reloc_t = Relocation::reloc_t;
						// FIRST: solve symbols if can be solved !
						auto symbol_resolver = [&mode, op_sz](string& symbol, const string& section, reloc_t reloc){
							if (constants.has(symbol)) {
								if (reloc != reloc_t::R_386_16)
									throw syntax_error("You cannot use relative relocation on absolute data");
								symbol = std::to_string(constants[symbol].value);
							}else if (symtable.has(symbol)) {
								if (reloc == reloc_t::R_386_16) {
									string memdump = sections[symtable[symbol].section].memdump();
									int off = 2 * symtable[symbol].offset;
#ifdef LITTLE_ENDIAN
									int num = 0;
									for (int i = op_sz * 2 - 2; i >= 0; i -= 2)
										num += std::stoi(memdump.substr(off + i, 2), nullptr, 16) << (i * 4);
									symbol = std::to_string(num);
#else
									symbol = std::to_string(std::stoi(memdump.substr(off, op_sz * 2), nullptr, 16));
#endif	
								} else if (reloc == reloc_t::R_386_PC16) {
									symbol = std::to_string((uint16_t)symtable[symbol].offset - (uint16_t)sections[section].counter);
								}
							}
								
							else { // not in symtable
								symtable[symbol] = Symbol{ "RELOC", 0xFFFF , false };
								relocations[symbol] = Relocation{ section, sections[section].counter, symtable[symbol].index, reloc };
								symbol = std::to_string(std::pow(2,op_sz * 8)-1);
							}
						};
						auto get_sym = [&datum, mode, &j](int i) -> string& {
							if ((CLEAR_SYM(mode,i) == (REGIND16(i) | REGIND(i))) || (CLEAR_SYM(mode,i) == (REGIND8(i) | REGIND(i))))
								return datum.values[j+1];
							else return datum.values[j];
						};


						// call appropriate symbol resolver relocator
						if (mode & SYMABS(i)) {
							symbol_resolver(get_sym(i), iter->section, reloc_t::R_386_16);
						} else if (mode & SYMREL(i)) {
							symbol_resolver(get_sym(i), iter->section, reloc_t::R_386_PC16);
						} else if (mode & SYMADR(i)) { // TODO: this has different implementation
							symbol_resolver(get_sym(i), iter->section, reloc_t::R_386_PC16);
						}
						mode = CLEAR_SYM(mode, i);


						if (mode == REGDIR(i) || mode == REGIND(i)) {
							op_desc |= GET_REG(datum.values[j]) << 1;
							if ((datum.flags & REDUCED(i)) && (datum.values[++j] == "h")) {
								op_desc |= 0x1;
							}
						} 
						
						sections[iter->section].bytes.operator<<(op_desc);  // pushing operator desecriptor to stream

						if (mode == IMMED(i)) {
							if (utils::bitsize(utils::sctoi(datum.values[j])) > 8 * op_sz)
								throw syntax_error(iter->line_num, iter->line, "Overflow");
							sections[iter->section].get_stream(op_sz) << utils::sctoi(datum.values[j]);
						} else if (mode == (REGIND16(i) | REGIND(i))) {
							sections[iter->section].dwords << utils::sctoi(datum.values[++j]);
						} else if (mode == (REGIND8(i) | REGIND(i))) {
							sections[iter->section].words << utils::sctoi(datum.values[++j]);
						}

					}
				}
				std::cout << '\n';
			}
		}

		std::ofstream fout (output_path);

		std::cout << relocations;
		std::cout << sections;
		std::cout << symtable;
		std::cout << constants;

		fout << relocations;
		fout << sections;
		fout << symtable;
	}


}

#endif