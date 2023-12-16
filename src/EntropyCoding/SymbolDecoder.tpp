#pragma once

#include <algorithm>
#include <ostream>

#include <type_traits>
#include <functional>

#include "isymbolstream.h"
#include "EntropyTypes.h"
#include "SymbolCode.tpp"
#include "obitwrapper.tpp"

// inherently nor copyable nor movable
template <size_t wordDepth, size_t codeOption, typename buffer_t, typename ostream_char_t = wchar_t>
class SymbolDecoder {
	typedef std::basic_ostream<ostream_char_t> ostream_t;

	std::function<void(buffer_t)> storeCallback {
		[&](buffer_t item) -> void {
			this->m_ostream.write((typename ostream_t::char_type*)&item, sizeof(item) / sizeof(typename ostream_t::char_type));
			this->encodedCounter += sizeof(item) * 8;
		}
	};

	SymbolCode<wordDepth, codeOption, true> code;
	size_t encodedCounter = -((std::make_signed<size_t>::type)(sizeof(buffer_t) * 8));
	ostream_t &m_ostream;
	obitwrapper<buffer_t> bitbuffer;

public:
	SymbolDecoder(ostream_t &ostream)
		: m_ostream(ostream), bitbuffer(storeCallback) {}
	// check if needed to explicitly define destructor (destructor call on reference)
	~SymbolDecoder() = default;

	SymbolDecoder(SymbolDecoder &other) = delete;
	SymbolDecoder &operator=(SymbolDecoder &other) = delete;

	size_t close() {
		this->encodedCounter -= this->bitbuffer.ocount();
		this->bitbuffer.flush();
		return this->encodedCounter;
	}

	// TODO: len parameter
	void translate(isymbolstream &input) {
		while (input) {
			this->nextSymbol(input.get());
		}
	}

private:
	void nextSymbol(size_t symbol) {
		SymbolEncoding currentSymbol = { wordDepth, code.mapping[symbol] };
		this->bitbuffer << currentSymbol;
	}
};
