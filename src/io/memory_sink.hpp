#pragma once

#include <deque>
#include <functional>

#include "exception.hpp"
#include "bpe/obitwrapper.tpp"

#include "sink.hpp"
#include "memory_protocol.hpp"

template <typename T>
class memory_sink : public sink<T> {
	// std::deque<T> buffer;

	struct memory_buffer_wrapper {
		std::deque<T> buffer;

		void store_word(T value) {
			this->buffer.push_back(value);
		}
	} buffer_wrapper;

	obitwrapper<T> boutput;
	bool terminated_early = false;
	bool use_fill = false;
	bool completed = false;
public:
	memory_sink() : 
			boutput(std::bind(&memory_buffer_wrapper::store_word, this->buffer_wrapper, std::placeholders::_1)) 
		{}
	// TODO: using bind object for callback may pose performance consequences
	obitwrapper<T>& get_bitwrapper() override {
		return this->boutput;
	}

	void finish_session() override {
		if (this->boutput.dirty()) {
			try {
				this->boutput.flush();
			} catch (const ccsds::bpe::byte_limit_exception& ex) {
				this->handle_early_termination();
			}
		}

		size_t byte_limit = this->boutput.get_byte_limit();
		size_t byte_count = this->boutput.get_byte_count();
		if (byte_count < byte_limit) {
			if (this->use_fill) {
				size_t remaining_count = byte_limit - byte_count;
				constexpr T fill_value = 0;
				constexpr std::byte fill_value_byte {0};

				while (remaining_count > sizeof(fill_value)) {
					this->boutput.flush_word(fill_value);
					remaining_count -= sizeof(fill_value);
				}

				while (remaining_count != 0) {
					this->boutput.flush_word(fill_value_byte);
					remaining_count -= sizeof(fill_value_byte);
				}

				byte_count = byte_limit;
			}
		}
	}


	void handle_early_termination() override {
		this->terminated_early = true;

		size_t byte_limit = this->boutput.get_byte_limit();
		size_t byte_count = this->boutput.get_byte_count();
		if (byte_count > byte_limit) {
			ptrdiff_t truncate_offset = ((ptrdiff_t)(byte_limit)) - byte_count;
			using buffer_word_t = decltype(this->buffer_wrapper.buffer)::value_type;
			constexpr size_t buffer_word_size_mask = sizeof(buffer_word_t) - 1;
			ptrdiff_t word_offset = truncate_offset >> std::bit_width(buffer_word_size_mask);

			truncate_offset |= ~buffer_word_size_mask;

			// auto start_it = this->buffer_wrapper.buffer.end() + word_offset;
			// auto last_word_view = std::as_writable_bytes(std::span<buffer_word_t, 1>(start_it, start_it + 1));
			// for (ptrdiff_t i = truncate_offset; i < 0; ++i) {
			// 	last_word_view[last_word_view.size() - i] = std::byte{ 0 };
			// 	reinterpret_cast<std::byte*>()
			// 	this->buffer_wrapper.buffer[this->buffer_wrapper.buffer.size() - word_offset];
			// }
			
			++word_offset;
			buffer_word_t& last_word = this->buffer_wrapper.buffer[this->buffer_wrapper.buffer.size() - word_offset];
			for (ptrdiff_t i = truncate_offset; i < 0; ++i) {
				reinterpret_cast<std::byte*>(&last_word)[i] = std::byte{ 0 };
			}

			std::fill_n(this->buffer_wrapper.buffer.end() + word_offset, -word_offset, (buffer_word_t)0);
		}

		this->completed = true;
	}

	bool if_terminated_early() const override { return this->terminated_early; }
	bool if_started() const override { return this->boutput.get_byte_count() > 0; }
	bool if_completed() const override { return this->completed; }

	sink_type get_type() const { return sink_type::memory; }
	bool if_supports_header_override() const override { return false; }
	bool if_supports_delayed_truncation() const override { return false; }

	std::span<std::byte> get_overridable_header_area(size_t target_header_size) override {
		// TODO: error handling/protocol violation, throw exception
		return std::span<std::byte>{};
	}

	void commit_overridable_header(std::span<std::byte> header_data) override {
		// TODO: error handling/protocol violation, throw exception
	}

protected:
	void set_truncation_params(size_t byte_limit, bool use_fill) override {
		// try {
		// 	this->boutput.set_byte_limit(byte_limit);
		// }
		// catch (const ccsds::bpe::byte_limit_exception& e) {
		// 	// truncate current buffer to match limit
		// 	auto& buffer = this->buffer_wrapper.buffer;
		// 	size_t index = byte_limit / sizeof(decltype(this->buffer_wrapper.buffer)::value_type); // TODO: decltype(auto) cuts references?
		// 	if (index < buffer.size()) {
		// 		auto erase_start_iter = buffer.cbegin() + index + 1;
		// 		buffer.erase(erase_start_iter, buffer.cend());
		// 	}
		// 	throw e;
		// }
		this->use_fill = use_fill;
		this->boutput.set_byte_limit(byte_limit);
	}

	void apply_params(const segment_settings& seg_params, const compression_settings& compr_params) override {
		// protocol header constitution is already defined by the moment
		if (compr_params.early_termination) {
			this->enable_segment_truncation(compr_params.seg_byte_limit, compr_params.use_fill);
		} else {
			this->disable_segment_truncation();
		}
	}
};
