#pragma once

#include <bit>
#include <type_traits>

#include "EntropyTypes.h"
#include "exception.h"

template <typename buffer_t> class ibitwrapper {
	typedef typename std::make_unsigned<buffer_t>::type ubuffer_t;
	typedef typename std::make_signed<buffer_t>::type sbuffer_t;

	buffer_t buffer = 0;
	const size_t capacity = sizeof(buffer_t) << 3;
	size_t rcount = capacity;

	size_t byte_limit; // TODO: but should be capable of holding 2^27 - 1. See 4.2.3.2.1
	size_t byte_count = 0;

	typedef const std::function<buffer_t(void)> callback_t;
	callback_t source;

public:
	ibitwrapper(const callback_t &callback, size_t src_byte_limit = ((1 << 27) - 1)) 
			: source(callback), byte_limit(src_byte_limit) {};
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

	ibitwrapper& operator>>(vlw_t& symbol) {
		size_t length = symbol.length;
		do {
			if (this->rcount == this->capacity) {
				this->fill();
			}
			size_t shift = std::min(length, this->capacity - this->rcount);
			symbol.value <<= shift;
			// TODO: check if should be casted to underlying type of vlw_t when promoting negative
			symbol.value ^= ((~(((ubuffer_t)((sbuffer_t)(-1))) >> shift)) & (this->buffer)) >> (this->capacity - shift);
			this->buffer <<= shift;
			this->rcount += shift;
			length -= shift;
		} while (length > 0);
		return *this;
	}

	size_t extract(size_t length) {
		size_t result_value = 0;
		do {
			if (this->rcount == this->capacity) {
				this->fill();
			}
			size_t shift = std::min(length, this->capacity - this->rcount);
			result_value <<= shift;
			// TODO: check if should be casted to underlying type of vlw_t when promoting negative
			result_value ^= ((~(((ubuffer_t)((sbuffer_t)(-1))) >> shift)) & (this->buffer)) >> (this->capacity - shift);
			this->buffer <<= shift;
			this->rcount += shift;
			length -= shift;
		} while (length > 0);
		return result_value;
	}

	template <typename T>
	T extract_masked(T mask) {
		// Kind of overload for extract(size_t) tolerant to shift param overflow.
		// Note that returned type is the same as paramter type
		//

		T result = 0;
		size_t length = std::popcount(mask);
		while (length > 0) {
			size_t step_len = std::min(length, this->capacity - 1);
			size_t step_result = this->extract(step_len);
			result << step_len;
			result |= step_result;
			length -= step_len;
		}
		return result;
	}

	size_t extract_next_one() {
		size_t result = 0;
		size_t step = std::countl_zero(this->buffer);
		// skip all zeroes buffers
		while (step >= (this->capacity - this->rcount)) {
			result += (this->capacity - this->rcount);
			this->fill();
			step = std::countl_zero(this->buffer);
		}
		// skip left zeroes
		this->buffer <<= step;
		this->rcount += step;
		if (this->rcount == this->capacity) {
			this->fill();
		}
		// skip trailing one bit
		this->buffer <<= 1;
		this->rcount += 1;

		return result + step;
	}

	void fill() {
		[[unlikely]]
		if (this->byte_count >= this->byte_limit) {
			throw ccsds::bpe::byte_limit_exception();
		}

		this->buffer = this->source();
		this->rcount = 0;
		this->byte_count += sizeof(decltype(this->buffer));
	}

	size_t icount() {
		// TODO
		return this->rcount;
	}
};