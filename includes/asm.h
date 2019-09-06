/*
	ASM: Assembler for a simple 16-bit 2-address processor
	Copyright (C) 2019 Aleksa Ilic <aleksa.d.ilic@gmail.com>
	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef __ASM_H__
#define __ASM_H__

#include <fstream>
#include "asm/parser.h"
#include "asm/source_iterator.h"
#include "asm/utils.h"
#include "asm/types.h"

namespace ASM {
	using string = std::string;
	using uint = unsigned int;
	template <typename T> using vector = std::vector<T>;

	namespace streams {
		auto& warning = std::cerr;
		auto& log = std::cout;
		auto& error = std::cerr;
	}

	hashvec<Symbol> symtable;
	hashvec<Section> sections;
	vector<Relocation> relocations;
	hashvec<Constant> constants;
	hashvec<Instruction, hashvec_traits_icase> optable = {
		{"nop", Nop},
		{"halt", Nop},
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

	struct TypeManager {
		virtual void onSkip(parsed_t& data) {}
		virtual void onAlign(parsed_t& data) {}
		virtual void onAlloc(parsed_t& data) {}
		virtual void onLabel(parsed_t& data) {}
		virtual void onSection(parsed_t& data) {}
		virtual void onReloc(parsed_t& data) {}
		virtual void onEqu(parsed_t& data) {}
		virtual void onWord(parsed_t& data) {}
		virtual void onInstruction(parsed_t& data) {}
	};

	//logging stream
	class Pass: protected TypeManager {
	protected:
		// helper function to fetch proper operand size
		static int get_op_sz(const string& instruction, const flags_t& flags) {
			if (!optable.has(instruction))
				throw std::runtime_error("Instruction not in optable");
			if (optable[instruction].flags & Nop) // no operands
				return 0;
			else if (optable[instruction].flags & E) // variable operands
				return flags & EXTENDED ? DWORD_SZ : WORD_SZ;
			else
				return DWORD_SZ;
		}
		string section = "UND";
	public:
		void process(const string& input_path) {
			streams::log << "pass starting: \n";

			for (source_iterator iter(input_path); (iter != EOF) || (iter->data[0].flags & END); ++iter) {
				streams::log << iter->section << ":\t";
				section = iter->section;
				for (auto& datum : iter->data) {
					//print parsed line on string
					for (auto& value : datum.values)
						streams::log << value << " || ";

					try {
						if (datum.flags & SKIP) onSkip(datum);
						else if (datum.flags & ALIGN) onAlign(datum);
						else if (datum.flags & ALLOC) onAlloc(datum);
						else if (datum.flags & LABEL) onLabel(datum);
						else if (datum.flags & SECTION) onSection(datum);
						else if (datum.flags & RELOC) onReloc(datum);
						else if (datum.flags & EQU) onEqu(datum);
						else if (datum.flags & WORD) onWord(datum);
						else if (datum.flags & INSTRUCTION) onInstruction(datum);
						else throw std::runtime_error("Irregular type, handler not provided");
					} catch (syntax_error& err) {
						streams::error << string_format("%s @ line:%d = %s", err.what(), iter->line_num, iter->line);
						std::terminate();
					} catch (std::exception& err) {
						streams::error << err.what();
						std::terminate();
					}

					streams::log << sections[iter->section].counter;
					
				}
				streams::log << '\n';
			}
			streams::log << "pass end.\n";
		}
	};

	class FirstPass: public Pass {
		void onSection(parsed_t& data) override {
			const std::string& section_name = data.values[0];
			// create section entry if it doesn't exist
			if (!sections.has(section_name))
				sections.put(section_name, Section{});
			// add symbol entry to symtable
			if (symtable.has(section_name))
				throw symbol_redeclaration("Section already exsits");
			symtable[section_name] = Symbol{ section_name, sections[section].counter };
		}
		void onLabel(parsed_t& data) override {
			if (symtable.has(data.values[0]))
				throw symbol_redeclaration("Label already declared");
			symtable[data.values[0]] = Symbol{ section, sections[section].counter };
		}
		void onInstruction(parsed_t& data) override {
			if (!optable.has(data.values[0]))
				throw syntax_error("Instruction doesn't exist");
			if (!(optable[data.values[0]].flags & E) && (data.flags & EXTENDED))
				throw syntax_error("This instruction has fixed size");

			int bytes = INSTR_SZ; // inital value for opcode
			int op_sz = get_op_sz(data.values[0], data.flags);

			auto ival = data.values.begin() + 1; // skipping instruction which is always first
			for (int i = 1; (i <= OP_NUM) && (data.flags & ENABLE(i)); i++, ival++) {
				bytes += 1; //op<num>_desc sz
				flags_t mode = MODE_MASK(data.flags, i);

				// error checking for improper size
				if ((data.flags & EXTENDED) && (data.flags & REDUCED(i)))
					throw syntax_error("You cannot use extended instruction with reduced register size");

				// skipping captured reduced if it exists
				if (data.flags & REDUCED(i))
					ival++;

				if (mode == IMMED(i) || mode == (IMMED(i) | SYMABS(i)))
					bytes += op_sz;
				else if (mode == (IMMED(i) | SYMREL(i)) || mode == (IMMED(i) | SYMADR(i)))
					bytes += DWORD_SZ;
				else if (mode == REGIND16(i)) {
					int shift_sz = utils::bitsize(utils::sctoi(*ival)) > WORD_SZ ? DWORD_SZ : WORD_SZ;
					if (shift_sz == WORD_SZ)
						SET_MODE(data.flags, i, REGIND8(i));
					bytes += shift_sz;
				}
				else if (mode == (REGIND16(i) | SYMABS(i))) {
					ival++;
					int shift_sz = constants.has(*ival) ? (utils::bitsize(utils::sctoi(*ival)) > WORD_SZ ? DWORD_SZ : WORD_SZ) : DWORD_SZ;
					if (shift_sz == WORD_SZ)
						SET_MODE(data.flags, i, REGIND8(i));
					bytes += shift_sz;
				}
				else if (mode == MEM(i))
					bytes += DWORD_SZ;

			}

			sections[section].counter += bytes;
		}
		void onAlloc(parsed_t& data) override {
			int multiplier = data.values[0] == "byte" ? WORD_SZ : DWORD_SZ;
			sections[section].counter += (data.values.size() - 1) * multiplier;
		}
		void onAlign(parsed_t& data) override {
			int num = std::stoi(data.values[0]);
			if (!(((num & ~(num - 1)) == num) ? num : 0))
				throw syntax_error("Align number must be power 2");
			sections[section].counter += sections[section].counter % num;
		}
		void onSkip(parsed_t& data) override {
			sections[section].counter += std::stoi(data.values[1]);
		}
		void onEqu(parsed_t& data) override {
			constants[data.values[0]].value = utils::sctoi(data.values[1]);
		}
	};

	class SecondPass : public Pass {
		using reloc_t = Relocation::reloc_t;

		void onAlloc(parsed_t& data) override {
			auto& stream = data.values[0] == "byte" ? sections[section].words : sections[section].dwords;
			for (auto it = ++data.values.begin(); it != data.values.end(); ++it) {
				stream << utils::sctoi(*it);
			}
		}
		void onReloc(parsed_t& data) override {
			if (symtable.has(data.values[1]))
				symtable[data.values[1]].isLocal = false;
		}
		void onSkip(parsed_t& data) override {
			for (int i = 0; i < std::stoi(data.values[1]); i++)
				sections[section].bytes << std::stoi(data.values.size() > 2 ? data.values[2] : "0");
		}
		void onInstruction(parsed_t& data) override {
			uint8_t instr_desc = optable[data.values[0]].index << 3;
			if (get_op_sz(data.values[0], data.flags) == DWORD_SZ)
				instr_desc |= 0x4;

			sections[section].bytes << instr_desc; // pushing instruction desecriptor to stream

			//if((datum.flags & ENABLE(2)) && (MODE_MASK(CLEAR_SYM(datum.flags),1) == IMMED(1)))
			//	throw syntax_error(iter->line_num, iter->line, "IMMED for dst not allowed.");

			auto ival = data.values.begin() + 1; //skipping instructions

			for (int i = 1; (i <= OP_NUM) && (data.flags & ENABLE(i)); i++, ival++) {
				// process operand info
				flags_t mode = MODE_MASK(data.flags, i);
				uint8_t op_desc = ADDR_MASK(data.flags, i);
				int op_sz = get_op_sz(data.values[0], data.flags);
				// override in case of regind with movement as it doesn't care for optable
				if (CLEAR_SYM(mode, i) == REGIND8(i))
					op_sz = WORD_SZ;
				else if (CLEAR_SYM(mode, i) == REGIND16(i))
					op_sz = DWORD_SZ;

				// helper capturing lambda functions for symbol resolvment
				auto get_sym = [&data, mode, &ival](int i) -> string& {
					if ((CLEAR_SYM(mode, i) == REGIND16(i)) || (CLEAR_SYM(mode, i) == REGIND8(i)))
						return *(ival + 1);
					else return *ival;
				};
				auto symbol_resolver = [op_desc, op_sz](string& symbol, const string& section, reloc_t reloc) {
					auto make_relocation = [&]() {
						// counter + 1 is dirty fix because symbol resolvment happens before opdesc is pushed to stream and that can never be subject to relocation as it is always known
						relocations.push_back(Relocation{ section, sections[section].counter + 1, symtable[symbol].index, reloc });
						symbol = std::to_string(std::pow(2, op_sz * 8) - 1);
					};

					if (constants.has(symbol)) {
						if (reloc != reloc_t::R_386_16)
							throw syntax_error("You cannot use relative relocation on absolute data");
						symbol = std::to_string(constants[symbol].value);
					} else if (symtable.has(symbol) && symtable[symbol].offset != 0xFFFF) {
						if (reloc == reloc_t::R_386_16) {
							string memdump = sections[symtable[symbol].section].memdump();
							int off = 2 * symtable[symbol].offset;
							// if mem has not yet been populated we cannot access it - must add relocation
							if (memdump.size() < off + op_sz * 2)
								return make_relocation();
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
					} else { // not in symtable
						symtable[symbol] = Symbol{ "RELOC", 0xFFFF , false };
						make_relocation();
					}
				};

				// call appropriate symbol resolver relocator
				if (mode & SYMABS(i)) {
					symbol_resolver(get_sym(i), section, reloc_t::R_386_16);
				} else if (mode & SYMREL(i)) {
					symbol_resolver(get_sym(i), section, reloc_t::R_386_PC16);
				} else if (mode & SYMADR(i)) { // TODO: this has different implementation
					symbol_resolver(get_sym(i), section, reloc_t::R_386_PC16);
				}

				// symbol resolvment is finished, clear unneeded flags
				mode = CLEAR_SYM(mode, i);

				if (mode == REGDIR(i) || mode == REGIND(i) || mode == REGIND16(i) || mode == REGIND8(i)) {
					op_desc |= GET_REG(*ival) << 1;
					if ((data.flags & REDUCED(i)) && (*++ival == "h")) {
						op_desc |= 0x1;
					}
				}
				sections[section].bytes << op_desc;  // pushing operator desecriptor to stream

				if (mode == IMMED(i)) {
					if (utils::bitsize(utils::sctoi(*ival)) > 8 * op_sz)
						throw syntax_error("Overflow");
					sections[section].get_stream(op_sz) << utils::sctoi(*ival);
				} else if (mode == REGIND16(i)) {
					sections[section].dwords << utils::sctoi(*++ival);
				} else if (mode == REGIND8(i)) {
					sections[section].words << utils::sctoi(*++ival);
				} else if (mode == MEM(i)) {
					sections[section].dwords << utils::sctoi(*ival);
				} 

			}
		}
	};

	string input_path, output_path;

	void init(const string& input, const string& output) {
		input_path = input;
		output_path = output;
	
		symtable = hashvec<Symbol>{};
		sections = hashvec<Section>{};
		relocations = vector<Relocation>{};
		constants = hashvec<Constant>{};
	}

	void assemble() {

		FirstPass{}.process(input_path);

		// restart section counters
		for (auto& section : sections)
			section.counter = 0;

		SecondPass{}.process(input_path);

		streams::log << relocations;
		streams::log << sections;
		streams::log << symtable;
		streams::log << constants;

		std::ofstream fout(output_path);
		fout << relocations;
		fout << sections;
		fout << symtable;
	}


}

#endif