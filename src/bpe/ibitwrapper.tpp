#pragma once

#include <span>
#include <bit>
#include <type_traits>

#include "utils.hpp"
#include "entropy_types.hpp"
#include "exception.hpp"

template <typename buffer_t> class ibitwrapper {
	typedef typename std::make_unsigned<buffer_t>::type ubuffer_t;
	typedef typename std::make_signed<buffer_t>::type sbuffer_t;

	buffer_t buffer = 0;
	const size_t capacity = sizeof(buffer_t) << 3;
	size_t rcount = capacity;

	size_t byte_limit = ((1 << 27) - 1); // TODO: but should be capable of holding 2^27 - 1. See 4.2.3.2.1
	size_t byte_count = 0;

	typedef std::function<buffer_t(void)> callback_t;
	callback_t source;

public:
	ibitwrapper(const callback_t& callback, size_t src_byte_limit = ((1 << 27) - 1)) 
			: source(callback)/*, byte_limit(src_byte_limit)*/ {};
	~ibitwrapper() = default;

	// Nor copyable nor movable
	ibitwrapper(const ibitwrapper &other) = delete;
	ibitwrapper& operator=(const ibitwrapper &other) = delete;

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
		size_t step = std::countl_zero((ubuffer_t)(this->buffer));
		// skip all zeroes buffers
		while (step >= (this->capacity - this->rcount)) {
			result += (this->capacity - this->rcount);
			this->fill();
			step = std::countl_zero((ubuffer_t)(this->buffer));
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
	
	void read_bytes(std::span<std::byte> data_bytes) {
		// data_bytes are assumed to be ordered MSB first

		constexpr size_t bindex_mask = ~((-1) << 3);
		if ((data_bytes.size() > sizeof(buffer_t)) & ((this->rcount & bindex_mask) == 0)) {
			// we're lucky and current buffer's fill counter is on byte 
			// boundary. At least we can copy whole bytes to the buffer.
			// 
			// check if data_bytes.data() is aligned properly next, if so we 
			// can copy whole words (maybe reversing order of bytes if 
			// environment is not big-endian (input is string and supposed to 
			// be organized in big-endian manner)

			ptrdiff_t index = 0;
			while (!this->empty()) {
				data_bytes[index] = std::byte{ (uint8_t)(this->extract(8)) };
				++index;
			}

			constexpr size_t alignment_mask = alignof(buffer_t) - 1;
			bool word_aligned = (((size_t)(data_bytes.data() + index)) & alignment_mask) == 0;
			while (index < (data_bytes.size() - sizeof(buffer_t))) {	// subtraction never underflows here
				bytes_view<buffer_t> buffer_word;
				buffer_word.compound = this->load_word();

				// this gonna be the only endiannes-dependent code in the whole
				// ibitwrapper
				if constexpr (std::endian::native != std::endian::big) {
					// data_bytes is MSB, that is, big-endian. If buffer word is little-endian, 
					// byteswap is needed to preserve the ordering in data_bytes.
					buffer_word.compound = byteswap(buffer_word.compound);
				}

				// can reasonably be well-predicted
				if (word_aligned) {
					*(reinterpret_cast<buffer_t*>(data_bytes.data() + index)) = buffer_word.compound;
					index += sizeof(buffer_t);
				} else {
					for (ptrdiff_t i = 0; i < sizeof(buffer_t); ++i) {
						data_bytes[index] = buffer_word.bytes[i];
						++index;
					}
				}
			}

			while (index != data_bytes.size()) {
				data_bytes[index] = std::byte{ (uint8_t)(this->extract(8)) };
				++index;
			}
		} else {
			for (std::byte& item : data_bytes) {
				item = std::byte{ (uint8_t)(this->extract(8)) };
			}
		}
	}

	size_t icount() {
		// TODO
		return this->rcount;
	}

	bool empty() const {
		return (this->rcount == this->capacity);
	}

	size_t get_byte_limit() const {
		// TODO: but are byte_limit checks even needed for ibitwrapper? Managing 
		// data provider would normally throw exception on end of data...

		return this->byte_limit;
	}

	size_t get_byte_count() const {
		return this->byte_count;
	}

	void set_byte_limit(int_least32_t limit, int_least32_t byte_count = 0) {

	}

	void set_byte_count(int_least32_t target_value) {
		bool valid = true;
		valid &= (target_value > 0);
		if (!valid) {

		}

		this->byte_count = target_value;
	}

private:
	void fill() {
		// [[unlikely]]
		// if (this->byte_count >= this->byte_limit) {
		// 	throw ccsds::bpe::byte_limit_exception();
		// }
		// 
		// this->buffer = this->source();
		// this->rcount = 0;
		// this->byte_count += sizeof(decltype(this->buffer));

		this->buffer = this->load_word();
		this->rcount = 0;
	}

	buffer_t load_word() {
		[[unlikely]]
		if (this->byte_count >= this->byte_limit) {
			throw ccsds::bpe::byte_limit_exception();
		}

		buffer_t result = this->source();
		this->byte_count += sizeof(decltype(this->buffer));
		return result;
	}
};
