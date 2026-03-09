#pragma once

#include <vector>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <functional>

#include "exception.hpp"
#include "utility.hpp"

#include "bpe/obitwrapper.tpp"
#include "io_data_registry.hpp"
#include "sink.hpp"


template <typename T>
struct file_buffer_wrapper {
	static constexpr size_t buffer_size = 0x100;

	std::vector<T> buffer;
	std::filebuf& file_stream;

	ptrdiff_t write_buffer_index = 0;
	ptrdiff_t data_max_index = buffer_size << 1;
	size_t buffer_index_mask = buffer_size - 1;

	file_buffer_wrapper(std::filebuf& stream);

	void store_word(T value);
	T load_word();

	void reset_buffer(); // noexcept?
	void flush_buffer();
	void fill_buffer();
};


template <typename T>
class file_sink: public sink<T> {
	file_descriptor& descriptor;
	// file_sink is non-copyable, 1-to-1 mapping of descriptor reference and file_sink 
	// object is maintained.
	//

	std::filebuf file_stream;
	file_buffer_wrapper<T> buffer_wrapper;
	obitwrapper<T> boutput;

	size_t bytes_written_oob = 0;
	size_t bytes_written_buffered = 0;

	bool terminated_early = false;
	bool completed = false;

	static constexpr size_t buffer_size = 0x100;
	static constexpr std::ios::openmode file_init_mode = std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc;
	static constexpr std::ios::openmode file_reopen_mode = std::ios::binary | std::ios::in | std::ios::out | std::ios::ate;

public:
	file_sink(file_descriptor& dst) :
			descriptor(dst),
			buffer_wrapper(file_stream), 
			boutput(std::bind_front(&file_buffer_wrapper<T>::store_word, std::ref(this->buffer_wrapper))) {}
	// TODO: boutput is constructed with bind object, may have performance consequences

	void finish_session(bool fill) override;
	void handle_early_termination() override;

	obitwrapper<T>& get_bitwrapper() override { return this->boutput; }

	bool if_terminated_early() const override { return this->terminated_early; }
	bool if_started() const override { return this->boutput.get_byte_count() > 0; }
	bool if_completed() const override { return this->completed; }

	storage_type get_type() const { return storage_type::file; }
	bool if_supports_header_override() const override { return true; }
	bool if_supports_delayed_truncation() const override { return true; }

	std::span<std::byte> get_overridable_header_area(size_t target_header_size) override;
	void commit_overridable_header(std::span<std::byte> header_data) override;

private:
	void init_resources() override;

	void reset_buffer();
	void flush_buffer();
	void finalize_output();
};

template <typename T>
class file_source : public source<T> {
private:
	const file_descriptor& descriptor;
	// file_source is non-copyable, 1-to-1 mapping of descriptor reference and file_source 
	// object is maintained.

	std::filebuf file_stream; 
	file_buffer_wrapper<T> buffer_wrapper;
	ibitwrapper<T> binput;	
	
	static constexpr std::ios::openmode file_init_mode = std::ios::binary | std::ios::in;
public:
	file_source(const file_descriptor& dst) : descriptor(dst), buffer_wrapper(file_stream),
		binput(std::bind_front(&file_buffer_wrapper<T>::load_word, std::ref(this->buffer_wrapper))) {}

	ibitwrapper<T>& get_bitwrapper() override { return this->binput; }
	storage_type get_type() const override { return storage_type::file; }
	void restart() override {}

private:
	void init_resources() override;
};

// desired usage scenarios and limitations:
//	protocol is initialized with segment data; header values heavily depend on segment statistics
//	bitwrapper and device buffers are managed by sink!
//	raw ccsds header should be accounted for seg_byte_limit!
//		but header extensions are not part of the segment and should not account for byte limit
//	header override should be supported for some combination of sinks and protocols:
//		such support requires direct access to sink device
//			network sink does not support header override at all
//			for memory sink it can be possible to modify header content if header parts set is not changed
//			file sink supports (and uses by default) any header override technique
//	specific settings of specific protocol extensions should be supported
//		and protocol extension settings should not be available for sinks that do not support them
// 
// 
// class responsibilities:
//	abstract sink: sink interface and basic invariant management constraints
//	base protocol: maintains normatice ccsds header invariant
//	sink: invariant management for sink device (and buffer)
//  extended protocol: maintains extended protocol data invariant
//		(thus should not make effort to modify base protocol state)
// 
// 
// sink responsibilities:
//		interface and implementation for writing protocol and segment data to the output device
//			protocol header override interface?
//				interface may have the form of obitwrapper& getter...
//					would not work for header data: some protocol data is OOB in terms of bitwrapper
//				or dedicated methods for protocol and segment objects as parameters
//		inhereted from obitwrapper high-end logic to handle early termination and fill behavior
//		device and buffer invariant
// 
//		
// it's problematic for sink to own protocol:
//	segment is needed for protocol initialization -> would need to own segment also
// 
//	table 1, implementation depends on:
//				sink	protocol	segment		encoder
// sink			  x								
// protocol					x			+			
// segment								x			
// encoder										  x
// 
// 
//	table 2: weak usage in implementation of interface (provides interface for):
//				sink	protocol	segment		encoder
// sink			  x								  +
// protocol					x						
// segment								x			
// encoder										  x
// 
//


// file_sink implementation section:
//

template <typename T>
void file_sink<T>::finish_session(bool fill) {
	if (this->boutput.dirty()) {
		try {
			this->boutput.flush();
		}
		catch (const ccsds::bpe::byte_limit_exception& ex) {
			this->handle_early_termination();
		}
	}
	this->flush_buffer();

	size_t byte_limit = this->boutput.get_byte_limit();
	size_t byte_count = this->boutput.get_byte_count();
	if (byte_count < byte_limit) {
		if (fill) {
			this->reset_buffer();
			size_t remaining_count = byte_limit - byte_count;
			auto& buffer = this->buffer_wrapper.buffer;

			while (remaining_count != 0) {
				size_t block_size = std::min(remaining_count,
					buffer.size() * sizeof(typename std::remove_reference_t<decltype(buffer)>::value_type));
				this->file_stream.sputn(reinterpret_cast<char*>(buffer.data()), block_size);
				remaining_count -= block_size;
			}
			byte_count = byte_limit;
		}
	}

	this->finalize_output();
}

template <typename T>
void file_sink<T>::handle_early_termination() {
	this->terminated_early = true;

	this->flush_buffer();
	// this->file_stream.overflow();

	size_t byte_limit = this->boutput.get_byte_limit();
	size_t byte_count = this->boutput.get_byte_count();
	if (byte_count > byte_limit) {
		ptrdiff_t truncate_offset = ((ptrdiff_t)(byte_limit)) - ((ptrdiff_t)(byte_count));
		this->file_stream.close();
		intmax_t current_size = std::filesystem::file_size(this->descriptor.path);
		std::filesystem::resize_file(this->descriptor.path, current_size + truncate_offset);
		this->file_stream.open(this->descriptor.path, file_reopen_mode);
	}

	this->finalize_output();
}

template <typename T>
std::span<std::byte> file_sink<T>::get_overridable_header_area(size_t target_header_size) {
	auto& buffer = this->buffer_wrapper.buffer;

	bool valid = true;
	valid &= (target_header_size <= (buffer.size() * sizeof(typename std::remove_reference_t<decltype(buffer)>::value_type)));
	if (!valid) {
		// TODO: error handling
	}

	this->reset_buffer();
	auto result = std::as_writable_bytes(
		std::span(buffer.begin(), buffer.end())).first(target_header_size);
	return result;
}

template <typename T>
void file_sink<T>::commit_overridable_header(std::span<std::byte> header_data) {
	bool valid = true;
	if (!valid) {
		// TODO: error handling
	}

	this->file_stream.pubseekpos(0);
	this->file_stream.sputn(reinterpret_cast<char*>(header_data.data()), header_data.size());
	// this->file_stream.overflow();
}

template <typename T>
void file_sink<T>::init_resources() {
	auto result = this->file_stream.open(this->descriptor.path, file_init_mode);
	if (result == nullptr) {
		// TODO: handle open file error
	}
}

template <typename T>
void file_sink<T>::reset_buffer() {
	this->buffer_wrapper.reset_buffer();
}

template <typename T>
void file_sink<T>::flush_buffer() {
	this->buffer_wrapper.flush_buffer();
}

template <typename T>
void file_sink<T>::finalize_output() {
	this->completed = true;
	// descriptor finalization logic...
}


// file_source implementation section:
//

template <typename T>
void file_source<T>::init_resources() {
	auto result = this->file_stream.open(this->descriptor.path, file_init_mode);
	if (result == nullptr) {
		// TODO: handle open file error
	}
}


// file_buffer_wrapper implementation section:
//

template <typename T>
file_buffer_wrapper<T>::file_buffer_wrapper(std::filebuf& stream) : buffer(buffer_size), file_stream(stream) {}

template <typename T>
void file_buffer_wrapper<T>::store_word(T value) {
	this->buffer[this->write_buffer_index] = value;
	++(this->write_buffer_index);

	[[unlikely]]
	if ((this->write_buffer_index & this->buffer_index_mask) == 0) {
		this->flush_buffer();
	}
	this->write_buffer_index &= this->buffer_index_mask;
}

template <typename T>
void file_buffer_wrapper<T>::reset_buffer() {
	constexpr T reset_item_value = 0;
	std::fill(this->buffer.begin(), this->buffer.end(), reset_item_value);
	this->write_buffer_index = 0;
}

template <typename T>
void file_buffer_wrapper<T>::flush_buffer() {
	if (std::endian::big != std::endian::native) {
		std::transform(this->buffer.cbegin(), this->buffer.cbegin() + this->write_buffer_index,
			this->buffer.begin(), byteswap<typename decltype(this->buffer)::value_type>);
	}
	auto wirtten_count = this->file_stream.sputn(reinterpret_cast<char*>(this->buffer.data()),
		this->write_buffer_index * sizeof(typename decltype(this->buffer)::value_type));
	// TODO: error handling on partial write / written_count < target size?
}


template <typename T>
T file_buffer_wrapper<T>::load_word() {
	[[unlikely]]
	if ((this->write_buffer_index & this->buffer_index_mask) == 0) {
		this->fill_buffer();
	}

	[[unlikely]]
	if (this->data_max_index == this->write_buffer_index) {
		// TODO: throw end of data exception?
	}

	T result = this->buffer[this->write_buffer_index];
	++(this->write_buffer_index);
	this->write_buffer_index &= this->buffer_index_mask;
	return result;
}

template <typename T>
void file_buffer_wrapper<T>::fill_buffer() {
	using word_t = decltype(this->buffer)::value_type;
	constexpr size_t buffer_capacity = buffer_size * sizeof(word_t);
	auto read_count = this->file_stream.sgetn(reinterpret_cast<char*>(this->buffer.data()),
		buffer_capacity);
	if (read_count < buffer_capacity) {
		size_t value_type_mask = sizeof(word_t) - 1;
		std::fill_n(reinterpret_cast<char*>(this->buffer.data()) + read_count,
			buffer_capacity - read_count, char{});
		this->data_max_index = (read_count + value_type_mask) >> std::bit_width(value_type_mask);
	}

	if constexpr (std::endian::native != std::endian::big) {
		size_t words_read_count = (read_count + sizeof(word_t) - 1) >> std::bit_width(sizeof(word_t) - 1);
		std::transform(this->buffer.cbegin(), this->buffer.cbegin() + words_read_count,
			this->buffer.begin(), byteswap<word_t>);
	}
}
