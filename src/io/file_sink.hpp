#pragma once

#include <vector>
#include <fstream>
#include <filesystem>
#include <bit>
#include <algorithm>
#include <functional>

#include "bpe/obitwrapper.tpp"
#include "utils.hpp"
#include "exception.hpp"

#include "sink.hpp"
#include "file_proto.hpp"

template <typename T>
class file_sink: public sink<T> {
	std::filesystem::path dst_path;
	// std::fstream file_stream;	// TODO: check filebuf as an alternative
	std::filebuf file_stream;

	struct file_buffer_wrapper {
		static constexpr size_t buffer_size = 0x100;

		std::vector<T> buffer;
		// std::fstream& file_stream;
		std::filebuf& file_stream;

		ptrdiff_t write_buffer_index = 0;
		size_t buffer_index_mask = buffer_size - 1;
		
		// file_buffer_wrapper(std::fstream& stream): buffer(buffer_size), file_stream(stream) {}
		file_buffer_wrapper(std::filebuf& stream): buffer(buffer_size), file_stream(stream) {}
		
		void store_word(T value) {
			this->buffer[this->write_buffer_index] = value;
			++(this->write_buffer_index);

			[[unlikely]]
			if ((this->write_buffer_index & this->buffer_index_mask) == 0) {
				this->flush_buffer();
			}
			this->write_buffer_index &= this->buffer_index_mask;
		}

		void reset_buffer() {
			constexpr T reset_item_value = 0;
			std::fill(this->buffer.begin(), this->buffer.end(), reset_item_value);
			this->write_buffer_index = 0;
		}

		void flush_buffer() {
			if (std::endian::big != std::endian::native) {
				std::transform(this->buffer.cbegin(), this->buffer.cbegin() + this->write_buffer_index,
					this->buffer.begin(), byteswap<decltype(this->buffer)::value_type>);
			}
			// this->file_stream.write(reinterpret_cast<char*>(this->buffer.data()), this->write_buffer_index * sizeof(decltype(this->buffer)::value_type));
			auto wirtten_count = this->file_stream.sputn(reinterpret_cast<char*>(this->buffer.data()), 
				this->write_buffer_index * sizeof(decltype(this->buffer)::value_type));
			// TODO: error handling on partial write / written_count < target size?
		}
	} buffer_wrapper;

	// std::vector<T> buffer;
	obitwrapper<T> boutput;

	size_t bytes_written_oob = 0;
	size_t bytes_written_buffered = 0;
	// ptrdiff_t write_buffer_index;
	// size_t buffer_index_mask;

	bool terminated_early = false;
	bool use_fill = false;
	bool completed = false;

	static constexpr size_t buffer_size = 0x100;
	static constexpr std::ios::openmode file_mode = std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc;

public:
	file_sink(const std::filesystem::path& target_path) : 
			dst_path(target_path), 
			// file_stream(dst_path, file_mode),	// TODO: lazy initialization is needed to cope with handle exhaustion
			buffer_wrapper(file_stream), 
			boutput(std::bind(&file_buffer_wrapper::store_word, this->buffer_wrapper, std::placeholders::_1)) {}
	// TODO: boutput is constructed with bind object, may have performance consequences

	void finish_session() override {
		if (this->boutput.dirty()) {
			try {
				this->boutput.flush();
			} catch (const ccsds::bpe::byte_limit_exception& ex) {
				this->handle_early_termination();
			}
		}
		this->flush_buffer();

		size_t byte_limit = this->boutput.get_byte_limit();
		size_t byte_count = this->boutput.get_byte_count();
		if (byte_count < byte_limit) {
			if (this->use_fill) {
				this->reset_buffer();
				size_t remaining_count = byte_limit - byte_count;
				auto& buffer = this->buffer_wrapper.buffer;
				
				while (remaining_count != 0) {
					size_t block_size = std::min(remaining_count, 
						buffer.size() * sizeof(std::remove_reference_t<decltype(buffer)>::value_type));
					// this->file_stream.write(reinterpret_cast<char*>(buffer.data()), block_size);
					this->file_stream.sputn(reinterpret_cast<char*>(buffer.data()), block_size);
					remaining_count -= block_size;
				}
				byte_count = byte_limit;
			}
		}

		// this->file_stream.flush();
		// this->file_stream.close();

		this->completed = true;
	}


	obitwrapper<T>& get_bitwrapper() override {
		return this->boutput;
	}

	void handle_early_termination() override {
		this->terminated_early = true;

		this->flush_buffer();
		// this->file_stream.flush();
		// this->file_stream.overflow();

		size_t byte_limit = this->boutput.get_byte_limit();
		size_t byte_count = this->boutput.get_byte_count();
		if (byte_count > byte_limit) {
			ptrdiff_t truncate_offset = ((ptrdiff_t)(byte_limit)) - byte_count;
			this->file_stream.close();
			size_t current_size = std::filesystem::file_size(this->dst_path);
			std::filesystem::resize_file(this->dst_path, current_size + truncate_offset);
			this->file_stream.open(this->dst_path, file_mode);
		}

		this->completed = true;
	}

	bool if_terminated_early() const override { return this->terminated_early; }
	bool if_started() const override { return this->boutput.get_byte_count() > 0; }
	bool if_completed() const override { return this->completed; }

	sink_type get_type() const { return sink_type::file; }
	bool if_supports_header_override() const override { return true; }
	bool if_supports_delayed_truncation() const override { return true; }

	std::span<std::byte> get_overridable_header_area(size_t target_header_size) override {
		auto& buffer = this->buffer_wrapper.buffer;

		bool valid = true;
		valid &= (target_header_size <= (buffer.size() * sizeof(std::remove_reference_t<decltype(buffer)>::value_type)));
		if (!valid) {
			// TODO: error handling
		}

		this->reset_buffer();
		auto result = std::as_writable_bytes(
			std::span(buffer.begin(), buffer.end())).first(target_header_size);
		return result;
	}

	void commit_overridable_header(std::span<std::byte> header_data) override {
		bool valid = true;
		if (!valid) {
			// TODO: error handling
		}

		// this->file_stream.seekp(0);
		// this->file_stream.write(reinterpret_cast<char*>(header_data.data()), header_data.size());
		// this->file_stream.flush();

		this->file_stream.pubseekpos(0);
		this->file_stream.sputn(reinterpret_cast<char*>(header_data.data()), header_data.size());
		// this->file_stream.overflow();
	}

protected:
	void apply_params(const segment_settings& seg_params, const compression_settings& compr_params) override {
		// protocol header constitution is already defined by the moment
		if (compr_params.early_termination) {
			this->enable_segment_truncation(compr_params.seg_byte_limit, compr_params.use_fill);
		} else {
			this->disable_segment_truncation();
		}
	}

	void set_truncation_params(size_t byte_limit, bool use_fill) override {
		constexpr size_t max_byte_limit = (1 << 27) - 1;
		if (byte_limit > max_byte_limit) {
			// value exceeds capacity of 27-bit field in header part 3
			// see 4.2.3.2.1
			// 
			// TODO: handle error
		}

		// TODO: but this implies T's size is pot, which may not be true
		constexpr size_t word_mask = (1 << std::bit_width(sizeof(T) - 1)) - 1;
		if ((byte_limit & word_mask) > 0) {
			// byte_limit is not multiple of sizeof(T)
			// TODO: handle error
		}

		constexpr size_t pre_p3_header_size = 9; // TODO: adjust if last, 8 or 9, depends
		if (byte_limit < pre_p3_header_size) {
			// byte_limit would be not large enough for the segment to handle 
			// header parts 1a, 1b and 2, and the setting would be not decoded 
			// reliably when decoding the segment (incomplete header)
			// see 4.2.3.2.1a
			// 
			// TODO: handle error
		}

		this->use_fill = use_fill;
		this->boutput.set_byte_limit(byte_limit);
	}

private:
	void init_resources() override {
		auto result = file_stream.open(this->dst_path, file_mode);
		if (result == nullptr) {
			// TODO: handle open file error
		}
	}
	
	// void store_word(T item) {
	// 	this->buffer[this->write_buffer_index] = item;
	// 	++(this->write_buffer_index);
	// 
	// 	[[unlikely]]
	// 	if ((this->write_buffer_index & this->buffer_index_mask) == 0) {
	// 		this->flush_buffer();
	// 	}
	// 	this->write_buffer_index &= this->buffer_index_mask;
	// }
	// 
	// void reset_buffer() {
	// 	constexpr T reset_item_value = 0;
	// 	std::fill(this->buffer.begin(), this->buffer.end(), reset_item_value);
	// 	this->write_buffer_index = 0;
	// }
	// 
	// void flush_buffer() {
	// 	if (std::endian::big != std::endian::native) {
	// 		std::transform(this->buffer.cbegin(), this->buffer.cbegin() + this->write_buffer_index,
	// 			this->buffer.begin(), byteswap<decltype(this->buffer)::value_type>);
	// 	}
	// 	this->file_stream.write(this->buffer.data(), this->write_buffer_index * sizeof(decltype(this->buffer)::value_type));
	// }

	void reset_buffer() {
		this->buffer_wrapper.reset_buffer();
	}

	void flush_buffer() {
		this->buffer_wrapper.flush_buffer();
	}
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

