#pragma once
#include <tuple>
#include <utility>
#include <span>
#include <cstddef>

#include "core_types.hpp"
#include "exception.hpp"

#include "io_settings.hpp"
#include "headers.hpp"
#include "bpe/obitwrapper.tpp"
#include "bpe/ibitwrapper.tpp"

class ccsds_protocol {
public:
	template <typename T>
	ccsds_protocol(const segment<T>& data, size_t segment_index);
	ccsds_protocol(std::span<std::byte> raw_header);

	void init_session(const session_settings& session_params, 
			const segment_settings& segment_params, const compression_settings& compression_params);
	void set_segment_params(const segment_settings& params);
	void set_compression_params(const compression_settings& params);
	void set_first(bool value);
	void set_last(size_t padding);

	bool if_first() const;
	bool if_last() const;

	bool if_contains_segment_params() const;
	bool if_contains_compression_params() const;
	bool if_contains_session_params() const;

	segment_settings get_segment_params() const;
	compression_settings get_compression_params() const;
	session_settings get_session_params() const;
	size_t get_pad_rows_count() const;

	size_t get_segment_count() const;
	size_t get_bit_depth_DC() const;
	size_t get_bit_depth_AC() const;

	virtual size_t header_size() const;

	virtual bool if_headers_overridable() const { return false; }
	
	std::span<std::byte> commit(std::span<std::byte> dst);

	template <typename T>
	void commit(obitwrapper<T>& dst, const compression_settings& params);

protected:
	virtual void commit_extension_headers(std::vector<std::span<const std::byte>>& header_collection) {}

public:
	static constexpr size_t max_header_size() noexcept;
	static constexpr size_t min_header_size() noexcept { return HeaderPart_1A::size(); }

private:
	template <typename T>
	using header_record_t = std::pair<T, bool>;
	// well, corresponding flags are present in header part 1 anyway, and 
	// header part 1 is always present, keeping dedicated bool flags is not 
	// necessary...
	// but may want to have bool flags for each header part to track which 
	// parts have already been commited/dumped to storage...

	std::tuple<
		header_record_t<HeaderPart_1A>,		// part 1a always present, see constructor
		header_record_t<HeaderPart_1B>,
		header_record_t<HeaderPart_2>,
		header_record_t<HeaderPart_3>,
		header_record_t<HeaderPart_4>> headers;
};

template <typename DerivedProtocolT>
struct ccsds_header_parser {
	using type = DerivedProtocolT;

	template <typename T>
	static DerivedProtocolT parse_header(ibitwrapper<T>& src);

	static_assert(std::is_base_of_v<ccsds_protocol, DerivedProtocolT> == true);
};


// ccsds_protocol implementation section:
//

template <typename T>
ccsds_protocol::ccsds_protocol(const segment<T>& data, size_t segment_index) {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	header_p1a.set_SegmentCount(segment_index);
	header_p1a.set_BitDepthDC(data.bdepthDc);
	header_p1a.set_BitDepthAC(data.bdepthAc);
}


template <typename T>
void ccsds_protocol::commit(obitwrapper<T>& dst, const compression_settings& params) {
	std::vector<std::span<const std::byte>> extension_headers;
	this->commit_extension_headers(extension_headers);

	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);

	constexpr size_t max_byte_limit = 1 << 27;
	size_t byte_limit = params.early_termination ? params.seg_byte_limit : max_byte_limit;

	bool valid = true;
	valid &= (byte_limit <= max_byte_limit);
	if (!valid) {
		// TODO: handle error
	}

	if (header_p1a.get_Part2Flag()) {
		constexpr size_t byte_limit_mask = (1 << 27) - 1;
		auto& [header, flag] = std::get<header_record_t<HeaderPart_2>>(this->headers);
			
		// byte_limit would be not large enough for the segment to handle 
		// header parts 1a, 1b and 2, and the setting would be not decoded 
		// reliably when decoding the segment (incomplete header)
		// see 4.2.3.2.1a
		// 
		// The standard provides an example that byte_limit value less than 
		// 9 (or 10 for the last segment) would be illegal, however any value 
		// that causes current segment's header to be trucated is illegal, 
		// since that would invalidate sequence-dependent settings for the 
		// following segments; e.g. it's illegal to truncate any header for 
		// the first segment in the session, because header part 4 gets truncated, 
		// that causes essential image metainfo to be lost and impossible to 
		// decode/recover.
		valid &= (byte_limit >= ccsds_protocol::header_size());
		valid &= ((byte_limit & byte_limit_mask) == header.get_SegByteLimit());
		if (!valid) {
			// TODO: handle error
		}
	}

	ptrdiff_t header_offset = ccsds_protocol::header_size() - this->header_size();
	dst.set_byte_limit(byte_limit, header_offset);

	for (auto& item : extension_headers) {
		dst.write_bytes(item);
	}

	auto write_header = [&]<typename hT>(bool if_present) -> void {
		if (if_present) {
			auto& [header, flag] = std::get<header_record_t<hT>>(this->headers);
			const auto& content = header.commit();
			dst.write_bytes(std::span(content));
			flag = true;
		}
	};

	dst.write_bytes(std::span(header_p1a.commit()));
	write_header.template operator()<HeaderPart_1B>(header_p1a.get_EndImgFlag());
	write_header.template operator()<HeaderPart_2>(header_p1a.get_Part2Flag());
	write_header.template operator()<HeaderPart_3>(header_p1a.get_Part3Flag());
	write_header.template operator()<HeaderPart_4>(header_p1a.get_Part4Flag());
}

constexpr size_t ccsds_protocol::max_header_size() noexcept {
	auto size_sum = []<typename... Headers>() consteval -> size_t {
		return ((Headers::size()) + ...);
	};

	return size_sum.template operator()<HeaderPart_1A, HeaderPart_1B, 
			HeaderPart_2, HeaderPart_3, HeaderPart_4>();
}


// ccsds_header_parser implementation section:
//

template <typename DerivedProtocolT>
template <typename T>
static DerivedProtocolT ccsds_header_parser<DerivedProtocolT>::parse_header(ibitwrapper<T>& src) {
	constexpr size_t max_byte_limit = 1 << 27;

	bool valid = true;
	valid &= (src.get_byte_limit() > DerivedProtocolT::min_header_size());
	valid &= (DerivedProtocolT::max_header_size() < max_byte_limit);	// sigh...?
	if (!valid) {
		// TODO: handle error?
	}

	size_t previous_size = 0;
	std::vector<std::byte> header_buffer(DerivedProtocolT::min_header_size());

	// make sure resize won't throw
	header_buffer.reserve(DerivedProtocolT::max_header_size());

	bool try_larger_header = true;
	try_larger_header &= (header_buffer.size() <= DerivedProtocolT::max_header_size());
	while (try_larger_header) {
		try {
			src.read_bytes(std::span(header_buffer).subspan(previous_size));
		} catch (const ccsds::bpe::byte_limit_exception& ex) {
			// TODO: review byte_limit design with input bitstreams
			header_buffer.resize(src.get_byte_count());
			try_larger_header = false;
		} // TODO: should we catch other exceptions with end of data meaning?

		try {
			DerivedProtocolT result(header_buffer);
			src.set_byte_count(result.ccsds_protocol::header_size());
			return result;
		} catch (const ccsds::io::truncated_header_exception& ex) {
			previous_size = header_buffer.size();
			if (ex.get_expected_size() > header_buffer.size()) {
				header_buffer.resize(ex.get_expected_size());
			} else {
				// this should be unreachable for valid data and valid target 
				// header implementation, but keep the branch for the sake of 
				// fault tolerance
				try_larger_header = false;
			}
		} catch (const ccsds::io::invalid_header_exception& ex) {
			// TODO: handle?
			throw;
		} 

		try_larger_header &= (header_buffer.size() <= DerivedProtocolT::max_header_size());
	}

	// segment data is too short to decode the header
	throw ccsds::io::invalid_header_exception{};
}
