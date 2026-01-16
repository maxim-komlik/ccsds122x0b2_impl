#pragma once
#include <tuple>
#include <utility>
#include <span>
#include <cstddef>

#include "core_types.hpp"

#include "io_settings.hpp"
#include "headers.hpp"

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

	virtual size_t header_size() const;

	virtual bool if_headers_overridable() const { return false; }
	virtual std::span<std::byte> commit(std::span<std::byte> dst) {
		return this->write_headers(dst);
	}

private:
	std::span<std::byte> write_headers(std::span<std::byte> dst);

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


template <typename T>
ccsds_protocol::ccsds_protocol(const segment<T>& data, size_t segment_index) {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	header_p1a.set_SegmentCount(segment_index);
	header_p1a.set_BitDepthDC(data.bdepthDc);
	header_p1a.set_BitDepthAC(data.bdepthAc);
}
