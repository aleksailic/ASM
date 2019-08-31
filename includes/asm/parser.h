#ifndef __PARSER_H__
#define __PARSER_H__

#include <string>
#include <vector>
#include <regex>
#include <algorithm>

#include "asm/types.h"
#include "asm/errors.h"

namespace ASM {
	using string = std::string;
	template <typename T> using vector = std::vector<T>;


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