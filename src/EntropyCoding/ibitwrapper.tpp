#pragma once

#include <type_traits>
#include "EntropyTypes.h"

template <typename buffer_t> class ibitwrapper {
	typedef typename std::make_unsigned<buffer_t>::type ubuffer_t;
	typedef typename std::make_signed<buffer_t>::type sbuffer_t;

	buffer_t buffer = 0;
	static const size_t capacity = sizeof(buffer_t) * 8;
	size_t rcount = capacity;

	typedef const std::function<buffer_t(void)> callback_t;
	callback_t &source;

public:
	ibitwrapper(callback_t &callback) : source(callback){};
	~ibitwrapper() = default;

	// Nor copyable nor movable
	ibitwrapper(const ibitwrapper &other) = delete;
	ibitwrapper& operator=(const ibitwrapper &other) = delete;

	ibitwrapper& operator>>(shift_params &symbol) {
		do {
			if (this->rcount == this->capacity) {
				this->fill();
			}
			size_t shift = std::min(symbol.length, this->capacity - this->rcount);
			symbol.code <<= shift;
			symbol.code ^= ((~(((ubuffer_t)((sbuffer_t)(-1))) >> shift)) & (this->buffer)) >> (this->capacity - shift);
			this->buffer <<= shift;
			this->rcount += shift;
			symbol.length -= shift;
		} while (symbol.length > 0);
		return *this;
	}

	void fill() {
		this->buffer = this->source();
		this->rcount = 0;
	}

	size_t icount() {
		// TODO
		return this->rcount;
	}
};