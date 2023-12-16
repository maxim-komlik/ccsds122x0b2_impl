#pragma once

#include <algorithm>
#include <istream>

#include <type_traits>
#include <functional>

#include "osymbolstream.h"
#include "EntropyTypes.h"
#include "SymbolCode.tpp"
#include "ibitwrapper.tpp"

// inherently nor copyable nor movable
template <size_t wordDepth, size_t codeOption, typename buffer_t, typename ostream_char_t = wchar_t>
class SymbolEncoder {
	typedef std::basic_istream<ostream_char_t> istream_t;

	std::function<buffer_t(void)> fetchCallback {
		[&]() -> buffer_t {
			buffer_t item;
			this->m_istream.read((typename istream_t::char_type*)&item, sizeof(item) / sizeof(typename istream_t::char_type));
			this->decodedCounter += sizeof(item) * 8;
			return item;
		}
	};

	SymbolCode<wordDepth, codeOption> code;
	size_t decodedCounter = -((std::make_signed<size_t>::type)(sizeof(buffer_t) * 8));
	istream_t &m_istream;
	ibitwrapper<buffer_t> bitbuffer;

public:
	SymbolEncoder(istream_t &istream)
		: m_istream(istream), bitbuffer(fetchCallback) {}
	// check if needed to explicitly define destructor (destructor call on reference)
	~SymbolEncoder() = default;

	SymbolEncoder(SymbolEncoder &other) = delete;
	SymbolEncoder &operator=(SymbolEncoder &other) = delete;

	size_t close() {
		this->decodedCounter -= this->bitbuffer.icount();
		return this->decodedCounter;
	}

	// TODO: len parameter
	void translate(osymbolstream &output) {
		while (this->m_istream) {
			output.put(this->nextSymbol());
		}
	}

private:
	size_t nextSymbol() {
		shift_params current_buf = { wordDepth, 0 };
		this->bitbuffer >> current_buf;
		return this->code.mapping[current_buf.code];
	}
};
