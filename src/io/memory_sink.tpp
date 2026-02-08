#pragma once

#include <deque>
#include <functional>

#include "exception.hpp"

#include "bpe/obitwrapper.tpp"
#include "io_data_registry.hpp"
#include "sink.hpp"

template <typename T>
class memory_sink : public sink<T> {
	memory_descriptor& descriptor;
	// file_sink is non-copyable, 1-to-1 mapping of descriptor reference and file_sink 
	// object is maintained.
	//

	struct memory_buffer_wrapper {
		std::deque<T> buffer;

		void store_word(T value) {
			this->buffer.push_back(value);
		}
	} buffer_wrapper;

	obitwrapper<T> boutput;
	bool terminated_early = false;
	bool completed = false;
public:
	memory_sink(memory_descriptor& dst) :
		descriptor(dst), 
		boutput(std::bind_front(&memory_buffer_wrapper::store_word, std::ref(this->buffer_wrapper))) {}
	// TODO: using bind object for callback may pose performance consequences

	void finish_session(bool fill) override;
	void handle_early_termination() override;

	obitwrapper<T>& get_bitwrapper() override { return this->boutput; }

	bool if_terminated_early() const override { return this->terminated_early; }
	bool if_started() const override { return this->boutput.get_byte_count() > 0; }
	bool if_completed() const override { return this->completed; }

	storage_type get_type() const { return storage_type::memory; }
	bool if_supports_header_override() const override { return false; }
	bool if_supports_delayed_truncation() const override { return false; }

	std::span<std::byte> get_overridable_header_area(size_t target_header_size) override {
		// TODO: error handling/protocol violation, throw exception
		return std::span<std::byte>{};
	}

	void commit_overridable_header(std::span<std::byte> header_data) override {
		// TODO: error handling/protocol violation, throw exception
	}

private:
	void finalize_output();
};

template <typename T>
class memory_source : public source<T> {
private:
	const memory_descriptor& descriptor;
	// file_source is non-copyable, 1-to-1 mapping of descriptor reference and file_source 
	// object is maintained.
	ibitwrapper<T> binput;

	static constexpr std::ios::openmode file_init_mode = std::ios::binary | std::ios::in;
public:
	memory_source(const memory_descriptor& dst) : descriptor(dst), 
		binput(std::bind_front(memory_source::get_load_fn(dst), this)) {}

	ibitwrapper<T>& get_bitwrapper() override { return this->binput; }
	storage_type get_type() const override { return storage_type::memory; }
	void restart() override {}

private:
	void init_resources() override {};

	T load_data_aligned() const {
		// TODO: implement?
		return T{};
	}

	T load_data_unaligned() const {
		// TODO: implement?
		return T{};
	}

	using load_fn_type = T (memory_source::*)(void) const;

	static load_fn_type get_load_fn(const memory_descriptor& target) {
		// having read_bytes interface for ibitwrapper, it becomes tricky to manage proper
		// external buffer offsets and alignment
		constexpr ptrdiff_t alignment_mask = alignof(T) - 1;
		bool word_aligned = (((ptrdiff_t)(target.data.data())) & alignment_mask) == 0;
		if (word_aligned) {
			return &memory_source::load_data_aligned;
		}
		return &memory_source::load_data_unaligned;
	}
};


// memory_sink implementation section
//

template <typename T>
void memory_sink<T>::finish_session(bool fill) {
	if (this->boutput.dirty()) {
		try {
			this->boutput.flush();
		}
		catch (const ccsds::bpe::byte_limit_exception& ex) {
			this->handle_early_termination();
		}
	}

	size_t byte_limit = this->boutput.get_byte_limit();
	size_t byte_count = this->boutput.get_byte_count();
	if (byte_count < byte_limit) {
		if (fill) {
			size_t remaining_count = byte_limit - byte_count;
			constexpr T fill_value = 0;
			constexpr std::byte fill_value_byte{ 0 };

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


template <typename T>
void memory_sink<T>::handle_early_termination() {
	this->terminated_early = true;

	size_t byte_limit = this->boutput.get_byte_limit();
	size_t byte_count = this->boutput.get_byte_count();
	if (byte_count > byte_limit) {
		ptrdiff_t truncate_offset = ((ptrdiff_t)(byte_limit)) - byte_count;
		using buffer_word_t = decltype(this->buffer_wrapper.buffer)::value_type;
		constexpr size_t buffer_word_size_mask = sizeof(buffer_word_t) - 1;
		ptrdiff_t word_offset = truncate_offset >> std::bit_width(buffer_word_size_mask);

		truncate_offset |= ~buffer_word_size_mask;

		++word_offset;
		buffer_word_t& last_word = this->buffer_wrapper.buffer[this->buffer_wrapper.buffer.size() - word_offset];
		for (ptrdiff_t i = truncate_offset; i < 0; ++i) {
			reinterpret_cast<std::byte*>(&last_word)[i] = std::byte{ 0 };
		}

		std::fill_n(this->buffer_wrapper.buffer.end() + word_offset, -word_offset, (buffer_word_t)0);
	}

	this->completed = true;
}

template <typename T>
void memory_sink<T>::finalize_output() {
	this->completed = true;
	// TODO: transform buffer to descriptor internal type and commit
}

// memory_source implementation section
//
