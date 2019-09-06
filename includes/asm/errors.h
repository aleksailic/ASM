#ifndef __ERRORS_H__
#define __ERRORS_H__

#include <exception>
#include <string>

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