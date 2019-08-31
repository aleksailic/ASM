#ifndef __SOURCE_ITERATOR_H__
#define __SOURCE_ITERATOR_H__

#include "parser.h"
#include "errors.h"
#include <fstream>

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

	class source_iterator {
		using iterator_category = std::input_iterator_tag;
		using value_type = string;
		using difference_type = std::ptrdiff_t;
		using pointer = string * ;
		using reference = string & ;
		using self_type = source_iterator;

		struct context_t {
			string section = "UND";
			vector<parsed_t> data;
			int line_num = 0;
			string line;
		};

		std::ifstream source;
		context_t context;
	public:
		source_iterator(string path) : source(path) { operator++(); }
		self_type& operator++(){
			// obtain new line from source and early exit if EOF reached
			if (!std::getline(source, context.line)) {
				context.line_num = EOF;
				return *this;
			}
			context.line_num++;

			// reset data on every new iteration
			context.data.clear();

			string line = context.line;
			for (auto& parser : parsers) {
				parsed_t data = parser.parse(line);
				if (data.flags & SUCCESS) { // if flags are not set than nothing worthy is on the line!
					// take out consumed data from line and leave only suffix
					line = data.values.back();
					// suffix no longer needed as it contains unparsed data
					data.values.pop_back();

					// put that data in our parsed data
					context.data.push_back(data);

					if (data.flags & SECTION)
						context.section = data.values[0];
					// if something other than label is parsed we are done with parsing. // TODO: make this property flagable and modular
					if (!(data.flags & LABEL))
						break;
				}
			}

			// if there are nonwhitespace characters not picked up by parsers that is syntax error
			if (!std::all_of(line.begin(), line.end(), isspace)) {
				throw syntax_error("Complete line was not processed. Leftover: " + line);
			}
				
			// skip empty lines as they don't do anything to source code
			return context.data.empty() ? operator++() : *this;
		}
		bool operator==(const self_type& rhs) { return STRUCT_EQ(context, rhs.context, line_num); }
		bool operator!=(const self_type& rhs) { return !STRUCT_EQ(context, rhs.context, line_num); }
		bool operator==(const int line_num) { return context.line_num == line_num; }
		bool operator!=(const int line_num) { return context.line_num != line_num; }
		context_t& operator*() {
			return context;
		}
		context_t* operator->() {
			return &context;
		}
	};
}

#endif
