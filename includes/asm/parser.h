#ifndef __PARSER_H__
#define __PARSER_H__

#include <string>
#include <vector>
#include <regex>

// Helper expanding macro that checks for partial struct equality given respectable struct's fields
#ifndef STRUCT_EQ
#ifndef COUNT_VARARGS
#define _GET_NTH_ARG(_1, _2, _3, _4, N, ...) N
#define COUNT_VARARGS(...) _GET_NTH_ARG(__VA_ARGS__, 4, 3, 2, 1)
#define CONCAT(a, b) a##_##b
#define CONCAT2(a, b) CONCAT(a, b)
#endif

#define STRUCT_EQ_FN(LHS, RHS, FIELD) (LHS.FIELD == RHS.FIELD)
#define STRUCT_EQ_1(LHS, RHS, FIELD) STRUCT_EQ_FN(LHS, RHS, FIELD) 
#define STRUCT_EQ_2(LHS, RHS, FIELD, ...) STRUCT_EQ_FN(LHS, RHS, FIELD) && STRUCT_EQ_1(LHS, RHS, __VA_ARGS__)
#define STRUCT_EQ_3(LHS, RHS, FIELD, ...) STRUCT_EQ_FN(LHS, RHS, FIELD) && STRUCT_EQ_2(LHS, RHS, __VA_ARGS__)
#define STRUCT_EQ(LHS,RHS, ...) CONCAT2(STRUCT_EQ, COUNT_VARARGS(__VA_ARGS__) )(LHS,RHS,__VA_ARGS__)
#endif

namespace ASM {
	using string = std::string;
	template <typename T> using vector = std::vector<T>;

#ifndef OP_NUM
#define OP_NUM 2
#define OP_DESC_SZ 8
#define OP_ADDR_SHIFT(n) OP_DESC_SZ*(OP_NUM-n) + 5

	// address modes
#define IMMED(n)	0x0 << OP_ADDR_SHIFT(n)
#define REGDIR(n)	0x1 << OP_ADDR_SHIFT(n)
#define REGIND(n)	0x2	<< OP_ADDR_SHIFT(n)
#define REGIND8(n)	0x3	<< OP_ADDR_SHIFT(n)
#define REGIND16(n)	0x4	<< OP_ADDR_SHIFT(n)
#define MEM(n)		0x5	<< OP_ADDR_SHIFT(n)
// symbol addressing modes
#define SYM(n)		0x1	<< OP_ADDR_SHIFT(n) - 2
#define SYMADDR(n)	0x3	<< OP_ADDR_SHIFT(n) - 2

#define REG(n)
#endif
	enum Types {
		EMPTY = 0x00 << OP_NUM * OP_DESC_SZ,
		LABEL = 0x80 << OP_NUM * OP_DESC_SZ,
		SECTION = 0x40 << OP_NUM * OP_DESC_SZ,
		RELOC = 0x20 << OP_NUM * OP_DESC_SZ,
		EQU = 0x10 << OP_NUM * OP_DESC_SZ,
		END = 0x08 << OP_NUM * OP_DESC_SZ,
		INSTRUCTION = 0x04 << OP_NUM * OP_DESC_SZ,
		EXTENDED = 0x02 << OP_NUM * OP_DESC_SZ,
		SUCCESS = 0x01 << OP_NUM * OP_DESC_SZ
	};
	typedef uint_fast32_t flags_t;

	struct parsed_t {
		flags_t flags;
		vector<string> values;
		parsed_t(flags_t flags, vector<string> values) : flags(flags), values(values) {}
	};

	struct parser {
		flags_t flags;
		vector<string> regexes;
		vector<vector<parser>> callbacks;

		parsed_t parse(const string& line) const {
			std::vector<string> values;
			flags_t flags = 0; // no flags initially as no match is default

			for (auto& regex : regexes) {
				std::smatch match;
				if (std::regex_search(line, match, std::regex(regex))) {
					// extract data from capture groups
					for (size_t i = 1; i < match.size(); i++) {
						values.push_back(match[i].str());
					}
					// sign this match with proper flag data
					flags = this->flags | SUCCESS;
					// append suffix to the end so next can access
					values.push_back(match.suffix());
					// add data from callbacks
					for (auto& callback_region : callbacks) {
						for (auto& callback : callback_region) {
							parsed_t cb_parsed = callback.parse(values.back());
							// success!
							if (cb_parsed.flags & SUCCESS) {
								flags |= callback.flags; //update working flags
								values.pop_back(); // remove last element
								values.insert(values.end(), cb_parsed.values.begin(), cb_parsed.values.end()); // append to working vector
								break; // no need to be running in this region anymore!
							}
						}
					}
				}
			}

			if (values.empty())
				values.push_back(line);

			return { flags,values };
		}
	};

	// definition of parsers that do regex magic
	const parser parsers[] = {
		{LABEL, {"^\\s*(\\w+):"}},
		{SECTION, {"\\.section\\s*\\\"\\.(\\w+)\\\"", "\\.(data)", "\\.(text)", "\\.(bss)"}},
		{RELOC, {"\\.(global|extern)\\s*([\\w,]+)"}},
		{EQU, {"\\.equ\\s*(\\w+),\\s*(\\w+)"}},
		{INSTRUCTION, {"\\s*(halt|xchg|int|mov|add|sub|mul|div|cmp|not|and|or|xor|test|shl|shr|push|pop|jmp|jeq|jne|jgt|call|ret|iret)"}, {
			{
				{EXTENDED, {"^(w)"}}
			},
			{
				{REGDIR(1), {"^\\s*r([0-7])", "^\\s*(ax)", "^\\s*(sp)", "^\\s*(bp)"}},
				{REGIND(1), {"^\\s*\\\[r([0-7])\\\]", "^\\s*(ax)", "^\\s*(sp)", "^\\s*(bp)"}, {{
					{REGIND16(1), {"^\\s*\\\[(\\d+)\\\]"}},
					{REGIND16(1) | SYM(1), {"^\\s*\\\[(\\w+)\\\]"}}
				}}},
				{SYM(1), {"^\\s*(\\w+)"}},
				{SYMADDR(1), {"^\\s*&(\\w+)"}},
				{MEM(1), {"^\\s*\\\*(\\d+)"}}
			},
			{
				{0, {"^\\s*,"}}
			},
			{
				{IMMED(2), {"^\\s*(\\d+)"}},
				{REGDIR(2), {"^\\s*r([0-7])", "^\\s*(ax)", "^\\s*(sp)", "^\\s*(bp)"}},
				{REGIND(2), {"^\\s*\\\[r([0-7])\\\]", "^\\s*(ax)", "^\\s*(sp)", "^\\s*(bp)"}, {{
					{REGIND16(2), {"^\\s*\\\[(\\d+)\\\]"}},
					{REGIND16(2) | SYM(2), {"^\\s*\\\[(\\w+)\\\]"}}
				}}},
				{SYM(2), {"^\\s*(\\w+)"}},
				{SYMADDR(2), {"^\\s*&(\\w+)"}},
				{MEM(2), {"^\\s*\\\*(\\d+)"}}
			}
		}},
		{END, {"\\.end"}}
	};

}


#endif