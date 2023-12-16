#pragma once

#include <functional>

#include "EntropyTypes.h"
#include "EndianCoder.tpp"

template <typename buffer_t> class obitwrapper {
	typedef typename std::make_unsigned<buffer_t>::type ubuffer_t;
	typedef typename std::make_signed<buffer_t>::type sbuffer_t;
	buffer_t buffer = 0;
	size_t capacity = sizeof(buffer_t) * 8;
	size_t wcount = capacity;

	typedef const std::function<void(buffer_t)> callback_t;
	callback_t &dest;

public:
	obitwrapper(callback_t &callback) : dest(callback){};
	~obitwrapper() {
		if (this->wcount < this->capacity) {
			this->dest(this->buffer);
		}
	};

	// Nor copyable nor movable
	obitwrapper(const obitwrapper &other) = delete;
	obitwrapper& operator=(const obitwrapper &other) = delete;

	obitwrapper& operator<<(SymbolEncoding symbol) {
		do {
			size_t shift = std::min(symbol.length, this->wcount);
			this->wcount -= shift;
			symbol.length -= shift;
			this->buffer ^= ((~(((sbuffer_t)(-1)) << shift)) & (symbol.code >> symbol.length)) << this->wcount;
			if (this->wcount == 0) {
				this->flush();
			}
		} while (symbol.length > 0);
		return *this;
	}

	void flush() {
		this->dest(this->buffer);
		this->wcount = this->capacity;
		this->buffer = 0;
	}

	size_t ocount() {
		return this->wcount;
	}
};