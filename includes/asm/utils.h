#ifndef __ASM_UTILS_H__
#define __ASM_UTILS_H__

#include <string>
#include <algorithm>
#include <exception>

namespace ASM {
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
		uint16_t sctoi(const std::string& str) {
			try {
				return std::stoi(str);
			}
			catch (std::invalid_argument& exception) {
				if (str.length() == 1 && std::isalpha(str[0]))
					return str[0];
				else if (str == "\\n") return '\n';
				else if (str == "\\t") return '\t';
				else throw exception;
			}
		}
		std::string tolower(std::string str) {
			std::transform(str.begin(), str.end(), str.begin(), ::tolower);
			return str;
		}

		template<typename ... Args>
		std::string string_format(const std::string& format, Args ... args) {
			size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
			std::unique_ptr<char[]> buf(new char[size]);
			snprintf(buf.get(), size, format.c_str(), args ...);
			return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
		}
	}
}

#endif