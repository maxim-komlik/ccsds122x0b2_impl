#pragma once

#include <vector>
#include <algorithm>
#include <istream>
#include <functional>

#include "EntropyTypes.h"
#include "osymbolstream.h"
#include "EntropyCode.tpp"
#include "ibitwrapper.tpp"

template <size_t wordDepth, size_t codeOption, typename buffer_t, typename ostream_char_t = wchar_t>
class EntropyDecoder {
	typedef std::basic_istream<ostream_char_t> istream_t;

	std::function<buffer_t(void)> fetchCallback {
		[&]() -> buffer_t {
			buffer_t item;
			this->m_istream.read((typename istream_t::char_type*)&item, sizeof(item) / sizeof(typename istream_t::char_type));
			this->decodedCounter += sizeof(item) * 8;
			return item;
		}
	};

	EntropyCode<wordDepth, codeOption> code;
	size_t decodedCounter = -((std::make_signed<size_t>::type)(sizeof(buffer_t) * 8));
	istream_t& m_istream;
	ibitwrapper<buffer_t> bitbuffer;

public:
	EntropyDecoder(istream_t& istream): m_istream(istream), bitbuffer(fetchCallback) {}
	~EntropyDecoder() = default;

	EntropyDecoder(EntropyDecoder& other) = delete;
	EntropyDecoder& operator=(EntropyDecoder& other) = delete;

	size_t close() {
		this->decodedCounter += bitbuffer.icount();
		return this->decodedCounter;
	}

	void translate(osymbolstream& output){
		while(this->m_istream){
            output.put(this->decodeSymbol());
		}
	}

private:
	// TODO: fast start decoding (fetch until not 0)
	size_t decodeSymbol() {
		constexpr size_t mapping_len = sizeof(this->code.mapping) / sizeof(SymbolEncoding);
		shift_params current_buf = {0, 0b0};
		size_t index = 0;
		current_buf.length = this->code.mapping[index].length;
		this->bitbuffer >> current_buf;
		while (current_buf.code != this->code.mapping[index].code && index++ < mapping_len) {
			if (this->code.mapping[index].length - this->code.mapping[index - 1].length){
				current_buf.length = this->code.mapping[index].length - this->code.mapping[index - 1].length;
				this->bitbuffer >> current_buf;
			}
		} 
		return index;
	}
};
