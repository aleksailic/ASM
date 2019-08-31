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
	class syntax_error : public std::exception {
		std::string m_msg;
	public:
		syntax_error(const std::string& msg = "") : m_msg("Invalid syntax detected. " + msg) {}
		virtual const char * what() const noexcept { return m_msg.c_str(); }
	};
	struct symbol_redeclaration : public syntax_error{
		symbol_redeclaration(const std::string& msg = "") : syntax_error("Symbol redecleration not allowed. " + msg) {}
	};
}
#endif