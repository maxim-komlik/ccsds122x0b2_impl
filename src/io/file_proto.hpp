#pragma once

// 0-7		segment offset
// 8-f		protocol format marker
// 10-3f	reserved (protocol options)
// 
// 40-7f	actual segment size in bytes as encoded in the file
// 
// 80-a7	reserved, padding up to usable segment header space
// a8-11f	header encoding space, maximum 19 bytes
// 120-... 	segment data
// 

// 0-3f		actual segment size in bytes
// 40-47	segment header offset
// 48-4f	protocol marker
// 50-67	reserved, may be used for protocol params
// 
// 68-ff	header encoding space, maximum 19 bytes
//

#include <vector>
#include <array>
#include <fstream>
#include <filesystem>

#include <span>
#include <cstddef>

#include "ccsds_protocol.hpp"

class ccsds_file_protocol: public ccsds_protocol {
	struct proto_header {
		uint8_t seg_size[8]; 			// 0-3f		actual segment size in bytes
		uint8_t headers_offset[1]; 		// 40-47	segment header offset
		uint8_t protocol_marker[1]; 	// 48-4f	protocol marker
		union {
			uint8_t raw[1];
			struct {
				uint8_t major : 4; 		// 50-53	protocol major version
				uint8_t minor : 4; 		// 54-57	protocol version
			} structured;
		} protocol_version;
		uint8_t proto_params[1]; 		// 58-5f	protocol params: 
			// first session segment flag
			// non-standard compression modes/extensions switches
		uint8_t reserved001[1]; 		// 58-67	reserved
		uint8_t headers_area[19]; 		// 68-ff	header encoding space, maximum 19 bytes

		proto_header() {
			this->protocol_marker[0] = 0x5e;
			this->protocol_version.structured.major = 0;
			this->protocol_version.structured.minor = 1;
			// TODO: and other array members` values remain uninitialized...
		}
	};

	static constexpr size_t file_header_size = sizeof(proto_header);

	// as the actual header allocation is managed outside, state tracking is 
	// invalidated if caller passes invalid pointers as an argument to member 
	// functions...
	//
	bool file_header_initialized = false;
	bool file_header_commited = false;
	bool seg_size_writen = false;
	bool proto_header_writen = false;

public:
	using header_stub_t = std::array<std::byte, file_header_size>;

	bool if_headers_overridable() const override { return true; }

	header_stub_t start_session() {
		constexpr std::byte byte_init_value { 0 };
		header_stub_t result{ byte_init_value };
		
		proto_header* header = new(result.data()) proto_header;

		this->write_proto_headers(header);

		this->file_header_initialized = true;
		return result;
	}

	void set_segment_size(std::byte* header_start, size_t segment_size_bytes) {
		proto_header* header = reinterpret_cast<proto_header*>(header_start);
		constexpr size_t byte_width = 8;
		constexpr size_t byte_mask = ~(((ptrdiff_t)(-1)) << byte_width);
		
		constexpr ptrdiff_t max_index =
			(sizeof(proto_header::seg_size) / sizeof(*(proto_header::seg_size))) - 1;
		// byteswap is not applicable here due to unknown alignment of 
		// header_start pointer
		for (ptrdiff_t i = 0; i < sizeof(proto_header::seg_size); ++i) {
			header->seg_size[max_index - i] = segment_size_bytes & byte_mask;
			segment_size_bytes <<= byte_width;
		}
		this->seg_size_writen = true;
	}

private:
	void write_proto_headers(proto_header* header) {
		size_t header_start_offset = sizeof(proto_header::headers_area) - ccsds_protocol::header_size();
		uint8_t* header_start = header->headers_area + header_start_offset;
		ccsds_protocol::write_headers(header_start);
		header->headers_offset[0] = header_start - reinterpret_cast<uint8_t*>(header);

		this->proto_header_writen = true;
	}
};
