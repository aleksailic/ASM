#ifndef __ASM_TYPES_H__
#define __ASM_TYPES_H__

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN
#endif

#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include "asm/utils.h"
#include "asm/hashvec.h"
#include "asm/errors.h"

namespace ASM {
	template <typename T>
	using vector	 = std::vector<T>;
	using string	 = std::string;
	using uint		 = unsigned int;
	using flags_t	 = uint_fast32_t;
	using settings_t = uint_fast8_t;


	constexpr auto WORD_SZ	= 1;
	constexpr auto DWORD_SZ = 2;
	constexpr auto INSTR_SZ = 1;
	constexpr auto OP_NUM	= 2;
	constexpr auto OP_DESC_SZ = 8;
	constexpr auto REG_NUM	= 7;

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
			if (num < 0 || num > REG_NUM)
				throw syntax_error("Invalid register number supplied");
			return num;
		}
		catch (std::invalid_argument& exception) {
			throw syntax_error("Invalid register number supplied");
		}
	}

	//3bits for addressing mode
	constexpr flags_t OP_ADDR_SHIFT(int n) { return OP_DESC_SZ * (OP_NUM - n) + 5; }
	//4bits for registers
	constexpr flags_t OP_REG_SHIFT(int n) { return OP_DESC_SZ * (OP_NUM - n); }

	// operand enable bit
	constexpr flags_t ENABLE(int n)		{ return 0x1 << OP_REG_SHIFT(n); }

	// address modes
	constexpr flags_t IMMED(int n)		{ return (0x0 << OP_ADDR_SHIFT(n)) | ENABLE(n); }
	constexpr flags_t REGDIR(int n)		{ return (0x1 << OP_ADDR_SHIFT(n)) | ENABLE(n); }
	constexpr flags_t REGIND(int n)		{ return (0x2 << OP_ADDR_SHIFT(n)) | ENABLE(n); }
	constexpr flags_t REGIND8(int n)	{ return (0x3 << OP_ADDR_SHIFT(n)) | ENABLE(n); }
	constexpr flags_t REGIND16(int n)	{ return (0x4 << OP_ADDR_SHIFT(n)) | ENABLE(n); }
	constexpr flags_t MEM(int n)		{ return (0x5 << OP_ADDR_SHIFT(n)) | ENABLE(n); }
	// symbol addressing modes
	constexpr flags_t SYMABS(int n)		{ return 0x10 << OP_REG_SHIFT(n); }
	constexpr flags_t SYMREL(int n)		{ return 0x08 << OP_REG_SHIFT(n); }
	constexpr flags_t SYMADR(int n)		{ return 0x04 << OP_REG_SHIFT(n); }
	// lower or higher mark set
	constexpr flags_t REDUCED(int n)	{ return 0x02 << OP_REG_SHIFT(n); }

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
		EQU			= 0x010 << OP_NUM * OP_DESC_SZ,
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

	enum {
		Z = 1 << 0,  //zero
		O = 1 << 1,  //overflow
		C = 1 << 2,  //carry
		N = 1 << 3,  //negative
		E = 1 << 4,  //extensible (variable operand size)
		Nop = 1 << 5,  //no operands
		Tr = 1 << 13, //timer
		Tl = 1 << 14, //terminal
		I = 1 << 15  //interrupt
	};

	struct Instruction {
		typedef uint16_t flags_t;
		flags_t flags = 0;
	};

	struct Symbol {
		string section;
		uint offset;
		bool isLocal = true;
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
		string memdump() const {
			std::ostringstream oss;
			for (auto& byte : data)
				oss << std::setfill('0') << std::setw(2) << std::hex << std::uppercase << (int)byte;
			return oss.str();
		}
		const stream bytes{ *this, 8 };
		const stream words{ *this, WORD_SZ * 8 };
		const stream dwords{ *this, DWORD_SZ * 8 };

		const stream& get_stream(int byte_number) {
			switch (byte_number) {
			case WORD_SZ: return words;
			case DWORD_SZ: return dwords;
			default: throw std::runtime_error("Illegal byte number passed");
			}
		}

	};



	// specialization for pretty hashvec output
	std::ostream& operator<<(std::ostream& stream, const hashvec<Symbol>& symbols) {
		stream << "#tabela simbola\n";
		stream << "#ime" << '\t' << "sek" << '\t' << "vr." << '\t' << "vid." << '\t' << "r.b." << '\n';
		for (const auto& symbol : symbols) {
			stream << symbol.key << '\t' << symbol.section << '\t' << symbol.offset << '\t' << (symbol.isLocal ? "local" : "global") << '\t' << symbol.index << '\n';
		}
		return stream;
	}
	std::ostream& operator<<(std::ostream& stream, const hashvec<Constant>& constants) {
		stream << "#tabela konstanti\n";
		stream << "#ime" << '\t' << "vr." << '\t' << "r.b." << '\n';
		for (const auto& constant : constants) {
			stream << constant.key << '\t' << constant.value << '\t' << constant.index << '\t' << '\n';
		}
		return stream;
	}
	std::ostream& operator<<(std::ostream& stream, const hashvec<Section>& sections) {
		for (const auto& section : sections) {
			if (section.counter == 0) continue;
			stream << "#." << section.key << " (" << section.counter << ")\n";
			string memdump = section.memdump();
			for (int i = 0; i + 1 < memdump.length(); i += 2)
				stream << memdump[i] << memdump[i + 1] << " ";
			stream << '\n';
		}
		return stream;
	}
	std::ostream& operator<<(std::ostream& stream, const std::vector<Relocation>& relocations) {
		std::unordered_map<std::string, std::vector<Relocation>> map;
		for (const auto& relocation : relocations)
			map[relocation.section].push_back(relocation);

		for (const auto& rel_section : map) {
			stream << "#.ret." << rel_section.first << '\n';
			stream << "#ofset" << '\t' << "tip" << "\t\t" << "vr[." << rel_section.first << "]:\t" << '\n';
			for (const auto& relocation : rel_section.second) {
				stream << "0x" << std::setfill('0') << std::setw(DWORD_SZ * 2) << std::hex << std::uppercase;
				stream << relocation.offset << '\t' << (relocation.type == Relocation::reloc_t::R_386_16 ? "R_386_16" : "R_386_PC16") << '\t' << std::dec << relocation.num << '\n';
			}
		}

		return stream;
	}
}

#endif