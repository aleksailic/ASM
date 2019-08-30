#ifndef __PARSER_H__
#define __PARSER_H__

#include <string>
#include <vector>
#include <regex>
#include <algorithm>

#include "asm/errors.h"

namespace ASM {
	using string = std::string;
	template <typename T> using vector = std::vector<T>;
	typedef uint_fast32_t flags_t;
	typedef uint_fast8_t  settings_t;


constexpr auto WORD_SZ = 1;
constexpr auto DWORD_SZ = 2;
constexpr auto INSTR_SZ = 1;
constexpr auto OP_NUM = 2;
constexpr auto OP_DESC_SZ = 8;
constexpr auto REG_NUM = 7;

int GET_REG(const string& name) {
	if (name == "ax") return 0;
	else if (name == "bx")	return 1;
	else if (name == "cx")	return 2;
	else if (name == "dx")	return 3;
	else if (name == "bp")	return 5;
	else if (name == "sp")	return 6;
	else if (name == "pc")	return 7;
	else try {
		int num = std::stoi(name);
		if(num < 0 || num > REG_NUM)
			throw syntax_error("Invalid register number supplied");
		return num;
	}catch(std::invalid_argument& exception) {
		throw syntax_error("Invalid register number supplied");
	}
}

//3bits for addressing mode
constexpr flags_t OP_ADDR_SHIFT(int n)	{ return OP_DESC_SZ * (OP_NUM - n) + 5;}
//4bits for registers
constexpr flags_t OP_REG_SHIFT(int n)	{ return OP_DESC_SZ * (OP_NUM - n);}

// operand enable bit
constexpr flags_t ENABLE(int n)		{ return 0x1 << OP_REG_SHIFT(n);}

// address modes
constexpr flags_t IMMED(int n)		{ return (0x0 << OP_ADDR_SHIFT(n)) | ENABLE(n);}
constexpr flags_t REGDIR(int n)		{ return (0x1 << OP_ADDR_SHIFT(n)) | ENABLE(n);}
constexpr flags_t REGIND(int n)		{ return (0x2 << OP_ADDR_SHIFT(n)) | ENABLE(n);}
constexpr flags_t REGIND8(int n)	{ return (0x3 << OP_ADDR_SHIFT(n)) | ENABLE(n);}
constexpr flags_t REGIND16(int n)	{ return (0x4 << OP_ADDR_SHIFT(n)) | ENABLE(n);}
constexpr flags_t MEM(int n )		{ return (0x5 << OP_ADDR_SHIFT(n)) | ENABLE(n);}
// symbol addressing modes
constexpr flags_t SYMABS(int n)		{ return 0x10 << OP_REG_SHIFT(n);}
constexpr flags_t SYMREL(int n)		{ return 0x08 << OP_REG_SHIFT(n);}
constexpr flags_t SYMADR(int n)		{ return 0x04 << OP_REG_SHIFT(n);}
// lower or higher mark set
constexpr flags_t REDUCED(int n)	{ return 0x02 << OP_REG_SHIFT(n);}

// operator descriptor mask
	constexpr flags_t MODE_MASK(flags_t flags, int op_num) {
		int mask = 0;
		mask |= 0xFD << OP_DESC_SZ * (OP_NUM - op_num);
		mask |= ENABLE(op_num);
		return flags & mask;
	}

	constexpr void CLEAR_ADDR(flags_t& flags, int op_num) {
		flags &= ~(0x7 << OP_ADDR_SHIFT(op_num));
	}

	constexpr void SET_MODE(flags_t& flags, int op_num, flags_t mode) {
		CLEAR_ADDR(flags, op_num);
		flags |= mode;
	}

	constexpr flags_t CLEAR_SYM(flags_t flags, int op_num) {
		return flags & (~(SYMABS(op_num) | SYMADR(op_num) | SYMREL(op_num)));
	}
	constexpr flags_t CLEAR_SYM(flags_t flags) {
		for (int i = 1; i <= OP_NUM; i++)
			flags &= ~(SYMABS(i) | SYMADR(i) | SYMREL(i));
		return flags;
	}


	// getting address mode bits from flag
	constexpr uint8_t ADDR_MASK(flags_t flags, int op_num) {
		flags >>= OP_DESC_SZ * (OP_NUM - op_num);
		return flags & 0xE0;
	}

	enum Types {
		NOFLAG		= 0x000 << OP_NUM * OP_DESC_SZ,
		END			= 0x800 << OP_NUM * OP_DESC_SZ,
		SKIP		= 0x400 << OP_NUM * OP_DESC_SZ,
		ALIGN		= 0x200 << OP_NUM * OP_DESC_SZ,
		ALLOC		= 0x100 << OP_NUM * OP_DESC_SZ,
		LABEL		= 0x080 << OP_NUM * OP_DESC_SZ,
		SECTION		= 0x040 << OP_NUM * OP_DESC_SZ,
		RELOC		= 0x020 << OP_NUM * OP_DESC_SZ,
		EQU			= 0x010	<< OP_NUM * OP_DESC_SZ,
		WORD		= 0x008 << OP_NUM * OP_DESC_SZ,
		INSTRUCTION = 0x004 << OP_NUM * OP_DESC_SZ,
		EXTENDED	= 0x002 << OP_NUM * OP_DESC_SZ,
		SUCCESS		= 0x001 << OP_NUM * OP_DESC_SZ
	};

	enum Settings {
		DEFAULT		= 0x0,	
		RECURSIVE	= 0x1,
		REQUIRED	= 0x2,
		OVERRIDE	= 0x4
	};

	struct parsed_t {
		flags_t flags;
		vector<string> values;
		parsed_t(flags_t flags, vector<string> values) : flags(flags), values(values) {}
	};

	struct parser {
		flags_t flags;
		vector<string> regexes;
		vector<vector<parser>> callbacks;
		settings_t settings = DEFAULT;

		parsed_t parse(const string& line) const {
			std::vector<string> values;
			flags_t flags = 0; // no flags initially as no match is default
			bool overriden = false;

			for (auto& regex : regexes) {
				std::smatch match;
				if (std::regex_search(line, match, std::regex(regex, std::regex_constants::icase))) {
					// extract data from capture groups
					for (size_t i = 1; i < match.size(); i++) {
						values.push_back(match[i].str());
					}

					// append suffix to the end as it is needed for recursion and callbacks
					values.push_back(match.suffix());

					// if recursive flag is passed repeat same regex set while its sucessful and append results
					if (settings & RECURSIVE) {
						parsed_t parsed = parse(values.back());
						if (parsed.flags & SUCCESS) {
							values.pop_back(); // remove last element
							values.insert(values.end(), parsed.values.begin(), parsed.values.end()); // append to working vector
						}
					}
					// add data from callbacks
					for (auto& callback_region : callbacks) {
						for (auto& callback : callback_region) {
							parsed_t cb_parsed = callback.parse(values.back());
							if (cb_parsed.flags & SUCCESS) {
								flags |= cb_parsed.flags; //update working flags
								values.pop_back(); // remove last element
								values.insert(values.end(), cb_parsed.values.begin(), cb_parsed.values.end()); // append to working vector
								overriden = callback.settings & OVERRIDE;
								break; // no need to be running in this region anymore!
							}
						}
					}

					// sign data with proper flags
					flags |= SUCCESS;
					if(!overriden)
						flags |= this->flags;
				}
			}

			if (values.empty())
				values.push_back(line);

			return { flags,values };
		}
	};

	// some utilities that help configuration
	static vector<string> operator+(const string& prepender, const vector<string>& vector) {
		std::vector<string> ret;
		std::transform(vector.begin(), vector.end(), std::back_inserter(ret), [&prepender](const string& str) {return prepender + str; });
		return ret;
	}
	static vector<string> operator+(const vector<string>& vector, const string& appender) {
		std::vector<string> ret;
		std::transform(vector.begin(), vector.end(), std::back_inserter(ret), [&appender](const string& str) {return str + appender; });
		return ret;
	}
	static parser ADDITIONAL_ELEMENT(const vector<string>& regexes, settings_t settings = 0, flags_t flags = 0) {
		return { flags, "^\\s*,\\s*" + regexes, {{}}, settings };
	}

	static const vector<string> NUMCHAR_REGEXES = { "\\s*(\\d+)", "\\s*'(\\w)'", "\\s*'(\\\\\\w)'" };
	static const vector<string> REGISTER_REGEXES = { "\\s*r([0-7])", "\\s*(ax)", "\\s*(sp)", "\\s*(bp)", "\\s*(pc)" };

	static vector<parser> ADDR_MODE_PARSERS(int op) {
		return {
			{REGDIR(op), "^\\s*" + REGISTER_REGEXES, {
				{
					{REDUCED(op), {"^(l|h)"}}
				},
				{
					{REGIND16(op), {"^\\s*\\\[(\\d+)\\\]"}, {{}}, OVERRIDE},
					{REGIND16(op) | SYMABS(op), {"^\\s*\\\[(\\w+)\\\]"}, {{}}, OVERRIDE}
				}
			}},
			{REGIND(op), "^\\s*\\\[" + REGISTER_REGEXES + "\\\]", {
				{
					{REDUCED(op), {"^(l|h)"}}
				},
				{
					{REGIND16(op), {"^\\s*\\\[(\\d+)\\\]"}, {{}}, OVERRIDE},
					{REGIND16(op) | SYMABS(op), {"^\\s*\\\[(\\w+)\\\]"}, {{}}, OVERRIDE}
				}
			}},
			{MEM(op), {"^\\s*\\\*(\\d+)"} },
			{IMMED(op), NUMCHAR_REGEXES},
			{IMMED(op) | SYMABS(op), {"^\\s*(\\w+)"} },
			{IMMED(op) | SYMREL(op), {"^\\s*\\$(\\w+)"} },
			{IMMED(op) | SYMADR(op), {"^\\s*&(\\w+)"} },
		};
	}

	// definition of parsers that do regex magic
	const parser parsers[] = {
		{LABEL, {"^\\s*(\\w+):"}},
		{ALLOC,  "^\\s*\\.(byte|word|dword)" + NUMCHAR_REGEXES, {{ADDITIONAL_ELEMENT(NUMCHAR_REGEXES, RECURSIVE)}}},
		{ALIGN,  {"^\\s*\\.(align)\\s*(\\d+)" }, {{ADDITIONAL_ELEMENT({"(\\d+)"})}} },
		{SKIP,  {"^\\s*\\.(skip)\\s*(\\d+)" }, {{ADDITIONAL_ELEMENT({"(\\d+)"})}} },
		{SECTION, {"^\\s*\\.section\\s*\\\"\\.(\\w+)\\\"", "\\.(data)", "\\.(text)", "\\.(bss)"}},
		{RELOC, {"^\\s*\\.(global|extern|globl)\\s*([\\w,]+)"}},
		{EQU, {"^\\s*\\.equ\\s*(\\w+),\\s*(\\d+)"}},
		{INSTRUCTION, {"^\\s*(halt|xchg|int|mov|add|sub|mul|div|cmp|not|and|or|xor|test|shl|shr|push|pop|jmp|jeq|jne|jgt|call|ret|iret)"}, {
			{
				{EXTENDED, {"^w"}}
			},
			ADDR_MODE_PARSERS(1),
			{
				{NOFLAG, {"^\\s*,"}, {ADDR_MODE_PARSERS(2)}}
			}
		}},
		{END, {"^\\s*\\.end"}}
	};

}


#endif