#pragma once

#include <algorithm>
#include <ostream>

#include <type_traits>
#include <functional>

#include "EntropyCode.tpp"
#include "EntropyTypes.h"
#include "isymbolstream.h"
#include "obitwrapper.tpp"

// template <size_t... code_params, typename ...buffer_params>
// class EntropyEncoder;

// inherently nor copyable nor movable
template <size_t wordDepth, size_t codeOption, typename buffer_t, typename ostream_char_t = wchar_t>
class EntropyEncoder {
	typedef std::basic_ostream<ostream_char_t> ostream_t;

	std::function<void(buffer_t)> fillCallback {
		[&](buffer_t item) -> void {
			this->m_ostream.write((typename ostream_t::char_type*)&item, sizeof(item) / sizeof(typename ostream_t::char_type));
			this->encodedCounter += sizeof(item) * 8;
		} 
	};

	EntropyCode<wordDepth, codeOption> code;
	size_t encodedCounter = 0;
	ostream_t &m_ostream;
	obitwrapper<buffer_t> bitbuffer;

public:
	EntropyEncoder(ostream_t &ostream) 
		: m_ostream(ostream), bitbuffer(fillCallback) {}
	// check if needed to explicitly define destructor (destructor call on reference)
	~EntropyEncoder() = default;

	EntropyEncoder(EntropyEncoder &other) = delete;
	EntropyEncoder &operator=(EntropyEncoder &other) = delete;

	void translate(isymbolstream &input) {
		while (input) {
			this->encodeSymbol(input.get());
		}
	}

	size_t close() {
		this->encodedCounter -= this->bitbuffer.ocount();
		this->bitbuffer.flush();
		return this->encodedCounter;
	}

private:
	void encodeSymbol(size_t index) {
		this->bitbuffer << this->code.mapping[index];
	}
};

//_wordDepth, _codeOption, _buffer_t, _stream_char_t
// template<size_t _wordDepth, int _codeOption, typename _buffer_t, typename _stream_char_t = wchar_t>

// template <
// 	size_t _wordDepth, int _codeOption, typename _buffer_t, typename _stream_char_t, 
// 	CodeType<_wordDepth, _codeOption, _buffer_t, _stream_char_t>>
// struct TestA {
// 	EntropyEncoder<type::wordDepth, type::codeOption, type::buffer_t, type::stream_char_t> sample;
// };

// template<size_t _wordDepth, int _codeOption, typename _buffer_t, typename _stream_char_t = wchar_t>
// class EntropyEncoder<CodeType<_wordDepth, _codeOption, _buffer_t, _stream_char_t>>;
// 
// class EntropyEncoder<T::wordDepth, T::codeOption, typename T::buffer_t, typename T::stream_char_t>;
// 
// template <typename T>
// struct TestA {
// 	typedef std::basic_ostream<typename T::stream_char_t> ostream_t;
// 	EntropyEncoder<T::wordDepth, T::codeOption, typename T::buffer_t, typename T::stream_char_t> sample;
// 
// 	TestA(ostream_t &ostream) : sample(ostream) {};
// };
