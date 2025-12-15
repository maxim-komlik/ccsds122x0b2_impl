#pragma once

#include <vector>
#include <fstream>
#include <filesystem>
#include <bit>
#include <algorithm>

#include "bpe/obitwrapper.tpp"
#include "utils.hpp"
#include "exception.hpp"

#include "sink.hpp"
#include "file_proto.hpp"

template <typename T>
class file_sink: public sink<T> {
	std::filesystem::path dst_path;
	std::fstream file_stream;
	obitwrapper<T> boutput;
	std::vector<T> buffer;

	size_t bytes_written_oob;
	size_t bytes_written_buffered;
	ptrdiff_t write_buffer_index;
	size_t buffer_index_mask;

	bool terminated_early = false;
	bool use_fill = false;
	ccsds_file_protocol proto;

	virtual void init_session() override {
		// write file header as unformatted output except ccsds header part.
		auto proto_headers = proto.start_session();
		size_t ccsds_header_size = proto.header_size();
		ptrdiff_t ccsds_header_offset = proto_headers.size() - ccsds_header_size;
		this->file_stream.write(proto_headers.data(), ccsds_header_offset);

		std::span proto_headers_view(proto_headers);
		try {
			this->boutput.write_bytes(proto_headers_view.subspan(ccsds_header_offset));
		} catch (const ccsds::bpe::byte_limit_exception& ex) {
			this->handle_early_termination();
		}
	};

	void process_segment(BitPlaneEncoder<T>& encoder, segment<T>& data) override {
		if (!this->terminated_early) {
			try {
				encoder.encode(data, this->boutput);
			} catch (const ccsds::bpe::byte_limit_exception& ex) {
				this->handle_early_termination();
			}
		}
	}

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
				
				while (remaining_count != 0) {
					size_t block_size = std::min(remaining_count, 
						this->buffer.size() * sizeof(decltype(this->buffer)::value_type));
					this->file_stream.write(this->buffer.data(), block_size);
					remaining_count -= block_size;
				}
				byte_count = byte_limit;
			}
		}

		this->file_stream.flush();

		// complement and override file header
		ccsds_file_protocol::header_stub_t header_storage; // TODO: initialize?
		this->file_stream.seekg(0, std::ios_base::beg);
		this->file_stream.read(header_storage.data(), header_storage.size());
		this->proto.set_segment_size(byte_count);
		// this->proto.commit();
		this->file_stream.seekp(0, std::ios_base::beg);
		this->file_stream.write(header_storage.data(), header_storage.size());

		this->file_stream.close();
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

		this->boutput.set_byte_limit(byte_limit, this->ptoto.header_size());
		this->use_fill = use_fill;
	}

	ccsds_protocol& get_protocol() override {
		return this->proto;
	}

private:
	void store_word(T item) {
		this->buffer[this->write_buffer_index] = item;
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
		this->file_stream.write(this->buffer.data(), this->write_buffer_index * sizeof(decltype(this->buffer)::value_type));
	}

	void handle_early_termination() {
		this->terminated_early = true;
		// TODO: handle?
	}
};
