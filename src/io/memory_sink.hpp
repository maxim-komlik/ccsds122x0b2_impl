#pragma once

#include <deque>

#include "exception.hpp"
#include "bpe/obitwrapper.tpp"

#include "sink.hpp"

template <typename T>
class memory_sink : public sink<T> {
	obitwrapper<T> boutput;
	std::deque<T> buffer;
public:
	memory_sink() : boutput(this->obitwrapper_callback) {

	}

	obitwrapper<T>& get_obitwapper() const override {
		return this->boutput;
	}

	void set_byte_limit(size_t limit) override {
		try {
			this->boutput.set_byte_limit(limit);
		}
		catch (const ccsds::bpe::byte_limit_exception& e) {
			// truncate current buffer to match limit
			size_t index = limit / sizeof(decltype(this->buffer)::value_type);
			if (index < buffer.size()) {
				auto erase_start_iter = this->buffer.cbegin() + index + 1;
				this->buffer.erase(erase_start_iter, this->buffer.cend());
			}
			throw e;
		}
	}
private:
	void obitwrapper_callback(T value) {
		buffer.push_back(value);
	}
};
