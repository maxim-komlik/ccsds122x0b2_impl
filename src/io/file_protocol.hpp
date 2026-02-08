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

#include <span>
#include <cstddef>

#include "ccsds_protocol.hpp"

// #include "bitfield.hpp"

// In C++20, there's no construct that allow to start lifetime of an object 
// inplace, having storage of std::bytes provided, omitting construction.
// struct is a class type and has corresponding lifetime requirements, 
// using result pointer from reinterpret_cast causes undefined behavior.
// C++23 start_lifetime_as could be used, however.
// 
// struct proto_header {
// 	uint8_t seg_size[8]; 			// 0-3f		actual segment size in bytes
// 	uint8_t headers_offset[1]; 		// 40-47	segment header offset
// 	uint8_t protocol_marker[1]; 	// 48-4f	protocol marker
// 	union {
// 		uint8_t raw[1];
// 		struct {
// 			uint8_t major : 4; 		// 50-53	protocol major version
// 			uint8_t minor : 4; 		// 54-57	protocol version
// 		} structured;
// 	} protocol_version;
// 	uint8_t proto_params[1]; 		// 58-5f	protocol params: 
// 	// first session segment flag
// 	// non-standard compression modes/extensions switches
// 	uint8_t reserved001[1]; 		// 58-67	reserved
// 	std::byte headers_area[19]; 	// 68-ff	header encoding space, maximum 19 bytes
// 
// 	proto_header() {
// 		this->protocol_marker[0] = 0x5e;
// 		this->protocol_version.structured.major = 0;
// 		this->protocol_version.structured.minor = 1;
// 		// TODO: and other array members` values remain uninitialized...
// 	}
// };

// struct FileHeader;
// 
// template <>
// struct bitfield_traits<FileHeader> {
// 	static constexpr fields_description_t fields {
// 		bitfield_description{"segment_size"sv, 			{0, 64}, 0ull},		// Flags initial segment in an image
// 		bitfield_description{"header_offset"sv, 		{64, 8}, 29u},		// Flags final segment in an image
// 		bitfield_description{"protocol_marker"sv, 		{72, 8}, 0x5eu},	// Segment counter value
// 		bitfield_description{"protocol_ver_major"sv, 	{80, 4}, 0u},		// Number of bits needed to represent DC coefficients in 2’s complement representation
// 		bitfield_description{"protocol_ver_minor"sv, 	{84, 4}, 1u},		// Number of bits needed to represent absolute value of AC coefficients in unsigned integer representation
// 		bitfield_description{"protocol_params"sv, 		{88, 8}, 0u},		// 1 bit reserved
// 		bitfield_description{"reserved_001"sv, 			{96, 8}, 0u}		// Indicates presence of Part 2 header
// 		// uint8_t headers_area[19]; 		// 68-ff	header encoding space, maximum 19 bytes
// 	};	// 104 bits = 13 bytes {+ 19 bytes of header area}
// };

class file_protocol_header {
private:
	struct {
		uint8_t seg_size[8]; 			// 0-3f		actual segment size in bytes
		uint8_t headers_offset[1]; 		// 40-47	segment header offset
		uint8_t protocol_marker[1]; 	// 48-4f	protocol marker
		union {
			uint8_t raw[1];
			struct {
				uint8_t major : 4; 		// 50-53	protocol major version
				uint8_t minor : 4; 		// 54-57	protocol version
			} structured; // TODO: bit fields are not portable, may allocate bits MSb or LSb
		} protocol_version;
		uint8_t proto_params[1]; 		// 58-5f	protocol params: 
		// first session segment flag
		// non-standard compression modes/extensions switches
		uint8_t reserved001[1]; 		// 58-67	reserved
		uint8_t headers_area[19]; 		// 68-ff	header encoding space, maximum 19 bytes
	} data[1];
public:
	static constexpr size_t header_size = sizeof(data);

public:
	file_protocol_header() {
		this->data[0].protocol_marker[0] = 0x5e;
		this->data[0].protocol_version.structured.major = 0;
		this->data[0].protocol_version.structured.minor = 1;
		this->data[0].reserved001[0] = 0;

		this->data[0].headers_offset[0] = 28;
		this->data[0].proto_params[0] = 0x0;

		std::span headers_area_view(this->data[0].headers_area);
		std::fill_n(headers_area_view.begin(), headers_area_view.size(), 0);

		std::span segment_size_view(this->data[0].seg_size);
		std::fill_n(segment_size_view.begin(), segment_size_view.size(), 0);
	}

	file_protocol_header(std::span<std::byte> raw_data) {
		bool valid = true;
		valid &= (raw_data.size() >= sizeof(*this));
		if (!valid) {
			// TODO: error handling
		}

		auto write_member_value = [&](auto& member) -> void {
			auto view = std::as_writable_bytes(std::span(member));
			std::copy_n(raw_data.begin(), view.size(), view.begin());
			raw_data = raw_data.subspan<decltype(view)::extent>();
		};

		write_member_value(this->data[0].seg_size);
		write_member_value(this->data[0].headers_offset);
		write_member_value(this->data[0].protocol_marker);
		write_member_value(this->data[0].protocol_version.raw);
		write_member_value(this->data[0].proto_params);
		write_member_value(this->data[0].reserved001);
		write_member_value(this->data[0].headers_area);

		valid &= (this->data[0].protocol_marker[0] == 0x5e);
		valid &= (this->data[0].protocol_version.structured.major == 0);
		// TODO: other fields validation? headers_offset value boundaries?
		// value of headers_offset is used in unsigned arithmetic, should validate to 
		// avoid UB
		// 

		if constexpr (dbg::protocol::if_disabled(dbg::protocol::mask_forward_compatibility)) {
			valid &= (this->data[0].protocol_version.structured.minor == 1);
			valid &= (this->data[0].reserved001[0] == 0);
		}

		if (!valid) {
			// parsed header is corrupted / not compatible
			// TODO: error handling
		}
	}

	std::span<std::byte> get_ccsds_header_storage(size_t expected_size = 0) {
		// TODO: review, this assumes no padding bytes are preseing in data struct
		constexpr size_t headers_area_min_size = 3; 	// at least 3 bytes for header part 1a
		constexpr size_t headers_area_max_size = sizeof(this->data[0].headers_area); 	// at most 19 bytes
		constexpr ptrdiff_t headers_area_offset = sizeof(*(this->data)) - 
			sizeof((*(this->data)).headers_area);
		constexpr ptrdiff_t headers_area_max_offset = sizeof(this->data) - headers_area_min_size - 1;

		bool valid = true;
		valid &= (this->data[0].headers_offset[0] >= headers_area_offset);
		valid &= (this->data[0].headers_offset[0] <= headers_area_max_offset);

		// TODO: PERF NOTE: try skip conditional move to weaken pipeline data dependencies, 
		// value is reused shortly after conditional assignment; hence valuepick instead of 
		// conditional move
		expected_size = valuepickpred(
			(sizeof(this->data) - this->data[0].headers_offset[0]), expected_size,
			(expected_size != 0));
		valid &= (expected_size >= headers_area_min_size);
		valid &= (expected_size <= headers_area_max_size);
		
		if (!valid) {
			// TODO: error handling
		}

		auto headers_view = std::as_writable_bytes(std::span(this->data[0].headers_area));

		this->data[0].headers_offset[0] = sizeof(this->data) - expected_size;
		return headers_view.last(expected_size);
	}

	void set_segment_size(size_t segment_size_bytes) {
		constexpr size_t byte_width = 8;
		constexpr size_t byte_mask = ~(((ptrdiff_t)(-1)) << byte_width);

		constexpr ptrdiff_t max_index =
			(sizeof(this->data[0].seg_size) / sizeof(*(this->data[0].seg_size))) - 1;
		
		for (ptrdiff_t i = 0; i <= max_index; ++i) {
			this->data[0].seg_size[max_index - i] = segment_size_bytes & byte_mask;
			segment_size_bytes >>= byte_width;
		}
	}

	std::span<const std::byte> commit(size_t expected_size = 0) {
		// TODO: review, this assumes no padding bytes are preseing in data struct
		constexpr size_t headers_area_min_size = 3; 	// at least 3 bytes for header part 1a
		constexpr size_t headers_area_max_size = sizeof(this->data[0].headers_area); 	// at most 19 bytes
		constexpr ptrdiff_t headers_area_offset = sizeof(*(this->data)) -
			sizeof((*(this->data)).headers_area);
		constexpr ptrdiff_t headers_area_max_offset = sizeof(this->data) - headers_area_min_size - 1;

		bool valid = true;
		valid &= (this->data[0].headers_offset[0] >= headers_area_offset);
		valid &= (this->data[0].headers_offset[0] <= headers_area_max_offset);

		// TODO: PERF NOTE: try skip conditional move to weaken pipeline data dependencies, 
		// value is reused shortly after conditional assignment; hence valuepick instead of 
		// conditional move
		expected_size = valuepickpred(
			(sizeof(this->data) - this->data[0].headers_offset[0]), expected_size,
			(expected_size != 0));
		valid &= (expected_size >= headers_area_min_size);
		valid &= (expected_size <= headers_area_max_size);

		if (!valid) {
			// TODO: error handling
		}

		size_t ccsds_headers_offset = sizeof(this->data) - expected_size;
		this->data[0].headers_offset[0] = ccsds_headers_offset;

		return std::as_bytes(std::span(this->data)).first(ccsds_headers_offset);
	}
};

class ccsds_file_protocol: private file_protocol_header, public ccsds_protocol {
public:
	template <typename T>
	ccsds_file_protocol(const segment<T>& data, size_t segment_index) :
			ccsds_protocol(data, segment_index) {}

	ccsds_file_protocol(std::span<std::byte> raw_header) : 
			file_protocol_header(raw_header),  
			ccsds_protocol(file_protocol_header::get_ccsds_header_storage()) {}

	using file_protocol_header::set_segment_size;

	bool if_headers_overridable() const override { return true; }
	size_t header_size() const override { return file_protocol_header::header_size; }

	using ccsds_protocol::commit;

public:
	static constexpr size_t max_header_size() noexcept { return file_protocol_header::header_size; };
	static constexpr size_t min_header_size() noexcept { return file_protocol_header::header_size; }

protected:
	void commit_extension_headers(std::vector<std::span<const std::byte>>& header_collection) override {
		header_collection.push_back(file_protocol_header::commit(ccsds_protocol::header_size()));
		// higher level protocols should call nearest-base commit non-virtually
	}
};
