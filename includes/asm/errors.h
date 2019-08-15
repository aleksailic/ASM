#ifndef __ERRORS_H__
#define __ERRORS_H__

#include <exception>
#include <string>

template<typename ... Args>
static std::string string_format(const std::string& format, Args ... args) {
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

namespace ASM {
	struct syntax_error : public std::exception {
		std::string m_msg;
		syntax_error(const std::string& msg = "") : m_msg("Invalid syntax detected. " + msg) {}
		syntax_error(int line_num, const std::string& line_data) {
			m_msg = string_format("Invalid syntax detected @ line:%d = %s", line_num, line_data.c_str() );
		}
		syntax_error(int line_num, const std::string& line_data, const std::string& msg) {
			m_msg = string_format("Invalid syntax detected. %s @ line:%d = %s", msg.c_str(), line_num, line_data.c_str());
		}
		virtual const char * what() const noexcept { return m_msg.c_str(); }
	};
	struct symbol_redeclaration : public syntax_error{
		symbol_redeclaration(int line_num, const std::string& line_data)
			: syntax_error(line_num, line_data, "Symbol redecleration not allowed") {}
		symbol_redeclaration(int line_num, const std::string& line_data, const std::string& msg)
			: syntax_error(line_num, line_data, "Symbol redecleration not allowed" + msg) {}
	};
}
#endif