#include "ccsds_protocol.hpp"

#include "utility.hpp"

ccsds_protocol::ccsds_protocol(std::span<std::byte> raw_data) {
	bool valid = true;
	valid &= (raw_data.size() >= HeaderPart_1A::size());
	if (!valid) {
		// TODO: error handling
	}

	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	header_p1a = HeaderPart_1A(raw_data.subspan<0, HeaderPart_1A::size()>());
	flag_p1a = true;

	raw_data = raw_data.subspan<HeaderPart_1A::size()>();

	auto parse_header = [&]<typename hT>(bool if_present) -> void {
		if (if_present) {
			valid &= (raw_data.size() >= hT::size());
			if (!valid) {
				// TODO: error handling
			}

			auto& [header, flag] = std::get<header_record_t<hT>>(this->headers);
			header = hT(raw_data.subspan<0, hT::size()>());
			flag = true;

			raw_data = raw_data.subspan<hT::size()>();
		}
	};

	parse_header.template operator()<HeaderPart_1B>(header_p1a.get_EndImgFlag());
	parse_header.template operator()<HeaderPart_2>(header_p1a.get_Part2Flag());
	parse_header.template operator()<HeaderPart_3>(header_p1a.get_Part3Flag());
	parse_header.template operator()<HeaderPart_4>(header_p1a.get_Part4Flag());

	if (header_p1a.get_Part4Flag()) {
		// part 4 requires that part 2 and part 3 are present as well

		auto& [header_p2, flag_p2] = std::get<header_record_t<HeaderPart_2>>(this->headers);
		auto& [header_p3, flag_p3] = std::get<header_record_t<HeaderPart_3>>(this->headers);

		valid &= (flag_p2 & flag_p3);
		if (!valid) {
			// TODO: error handling
		}
	}
}

void ccsds_protocol::init_session(const session_settings& session_params,
		const segment_settings& segment_params, const compression_settings& compression_params) {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	header_p1a.set_StartImgFlag(true);

	auto& [header_p4, flag_p4] = std::get<header_record_t<HeaderPart_4>>(this->headers);
	header_p4.set_DWTtype(session_params.dwt_type);
	header_p4.set_SignedPixels(session_params.signed_pixel);
	header_p4.set_PixelBitDepth(session_params.pixel_bdepth);
	// header_p4.set_ImageWidth(session_params.meta.width); // TODO: image width in session settings refactored?
	header_p4.set_ImageWidth(session_params.img_width);
	header_p4.set_TransposeImg(session_params.transpose);
	if (session_params.dwt_type == dwt_type_t::idwt) {
		header_p4.set_CustomWt(session_params.shifts);
	} else {
		header_p4.set_CustomWtFlag(false);
	}
	header_p1a.set_Part4Flag(true);

	this->set_segment_params(segment_params);
	this->set_compression_params(compression_params);
}

void ccsds_protocol::set_first(bool value) {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);

	if (value == false) {
		if (header_p1a.get_Part4Flag()) {
			// ?
			// TODO: cannot unset first flag for the first segment in a session
		}
	}
	header_p1a.set_StartImgFlag(value);
}

void ccsds_protocol::set_last(size_t padding) {
	auto& [header_p1b, flag_p1b] = std::get<header_record_t<HeaderPart_1B>>(this->headers);
	header_p1b.set_PadRows(padding);

	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	header_p1a.set_EndImgFlag(true);
}

bool ccsds_protocol::if_first() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	return header_p1a.get_StartImgFlag();
}

bool ccsds_protocol::if_last() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	return header_p1a.get_EndImgFlag();
}

void ccsds_protocol::set_segment_params(const segment_settings& params) {
	auto& [header_p3, flag_p3] = std::get<header_record_t<HeaderPart_3>>(this->headers);
	header_p3.set_S(params.size);
	header_p3.set_OptDCSelect(!params.heuristic_quant_DC);
	header_p3.set_OptACSelect(!params.heuristic_bdepth_AC);

	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	header_p1a.set_Part3Flag(true);
}

void ccsds_protocol::set_compression_params(const compression_settings& params) {
	auto& [header_p2, flag_p2] = std::get<header_record_t<HeaderPart_2>>(this->headers);
	header_p2.set_SegByteLimit(params.seg_byte_limit);
	header_p2.set_UseFill(params.use_fill);

	if (params.early_termination) {
		header_p2.set_DCStop(params.DC_stop);
		header_p2.set_BitPlaneStop(params.bplane_stop);
		header_p2.set_StageStop(params.stage_stop);
	} else {
		header_p2.set_DCStop(false);
		header_p2.set_BitPlaneStop(0);
		header_p2.set_StageStop(0b11);		// TODO: enum type for stages, because here stage 4 is meant to be used
	}

	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	header_p1a.set_Part2Flag(true);
}

bool ccsds_protocol::if_contains_segment_params() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	return header_p1a.get_Part3Flag();
}

bool ccsds_protocol::if_contains_compression_params() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	return header_p1a.get_Part2Flag();
}

bool ccsds_protocol::if_contains_session_params() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	return header_p1a.get_Part4Flag();
}

segment_settings ccsds_protocol::get_segment_params() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);

	bool valid = true;
	valid &= header_p1a.get_Part3Flag();
	if (!valid) {
		// TODO: error handling. throw.
	}

	segment_settings result;

	auto& [header_p3, flag_p3] = std::get<header_record_t<HeaderPart_3>>(this->headers);
	result.size = header_p3.get_S();
	result.heuristic_quant_DC = !header_p3.get_OptDCSelect();
	result.heuristic_bdepth_AC = !header_p3.get_OptACSelect();

	return result;
}

compression_settings ccsds_protocol::get_compression_params() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);

	bool valid = true;
	valid &= header_p1a.get_Part2Flag();
	if (!valid) {
		// TODO: error handling. throw.
	}

	constexpr size_t max_byte_limit = 1 << 27;

	compression_settings result;

	auto& [header_p2, flag_p2] = std::get<header_record_t<HeaderPart_2>>(this->headers);
	result.seg_byte_limit = header_p2.get_SegByteLimit();
	result.bplane_stop = header_p2.get_BitPlaneStop();
	result.stage_stop = header_p2.get_StageStop();
	result.DC_stop = header_p2.get_DCStop();
	result.use_fill = header_p2.get_UseFill();

	result.early_termination = !(
		(result.DC_stop == false) &
		(result.bplane_stop == 0) &
		(result.stage_stop == to_underlying(bpe_stage_index_t::stage_4)));

	result.seg_byte_limit += max_byte_limit & (size_t)(-(result.seg_byte_limit == 0));

	return result;
}

session_settings ccsds_protocol::get_session_params() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);

	bool valid = true;
	valid &= header_p1a.get_Part4Flag();
	if (!valid) {
		// TODO: error handling. throw.
	}

	session_settings result;

	auto& [header_p4, flag_p4] = std::get<header_record_t<HeaderPart_4>>(this->headers);
	result.img_width = header_p4.get_ImageWidth();
	result.pixel_bdepth = header_p4.get_PixelBitDepth();
	result.signed_pixel = header_p4.get_SignedPixels();
	result.transpose = header_p4.get_TransposeImg();
	result.dwt_type = header_p4.get_DWTtype();
	result.codeword_size = parse_codeword_length_value(header_p4.get_CodeWordLength());
	result.rows_pad_count = 0; // parsing postponed, will be set from HeaderPart_1B of the last segment

	if (header_p4.get_CustomWtFlag()) {
		result.shifts = header_p4.get_CustomWt();
	} else {
		if (result.dwt_type == dwt_type_t::idwt) {
			result.shifts = { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };
		} else {
			result.shifts = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
		}
	}

	return result;
}

size_t ccsds_protocol::get_pad_rows_count() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);

	bool valid = true;
	valid &= header_p1a.get_EndImgFlag();
	if (!valid) {
		// TODO: error handling. throw.
	}

	auto& [header_p1b, flag_p1b] = std::get<header_record_t<HeaderPart_1B>>(this->headers);
	return header_p1b.get_PadRows();
}

size_t ccsds_protocol::get_segment_count() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	return header_p1a.get_SegmentCount();
}

size_t ccsds_protocol::get_bit_depth_DC() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	return header_p1a.get_BitDepthDC();
}

size_t ccsds_protocol::get_bit_depth_AC() const {
	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	return header_p1a.get_BitDepthAC();
}

size_t ccsds_protocol::header_size() const {
	size_t result = 0;
	const auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	result += header_p1a.size();

	auto count_header_size = [&]<typename hT>(bool if_present) -> void {
		if (if_present) {
			const auto& [header, flag] = std::get<header_record_t<hT>>(this->headers);
			result += header.size();
		}
	};
	count_header_size.template operator()<HeaderPart_1B>(header_p1a.get_EndImgFlag());
	count_header_size.template operator()<HeaderPart_2>(header_p1a.get_Part2Flag());
	count_header_size.template operator()<HeaderPart_3>(header_p1a.get_Part3Flag());
	count_header_size.template operator()<HeaderPart_4>(header_p1a.get_Part4Flag());

	return result;
}

std::span<std::byte> ccsds_protocol::commit(std::span<std::byte> dst) {
	bool valid = true;
	valid &= (dst.size() >= this->header_size());
	valid &= this->if_headers_overridable();
	if (!valid) {
		// TODO: error handling
	}

	std::vector<std::span<const std::byte>> extension_headers;
	this->commit_extension_headers(extension_headers);

	auto current_header_area = dst;

	for (auto& item : extension_headers) {
		std::copy_n(item.begin(), item.size(), current_header_area.begin());
		current_header_area = current_header_area.subspan(item.size());
	}

	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	const auto& content_p1a = header_p1a.commit();
	std::copy_n(content_p1a.cbegin(), content_p1a.size(), current_header_area.begin());

	current_header_area = current_header_area.subspan<HeaderPart_1A::size()>();

	auto write_header = [&]<typename hT>(bool if_present) -> void {
		if (if_present) {
			valid &= (current_header_area.size() >= hT::size());
			if (!valid) {
				// TODO: error handling
			}

			auto& [header, flag] = std::get<header_record_t<hT>>(this->headers);
			const auto& content = header.commit();
			std::copy_n(content.cbegin(), content.size(), current_header_area.begin());
			flag = true;

			current_header_area = current_header_area.subspan<hT::size()>();
		}
	};

	write_header.template operator()<HeaderPart_1B>(header_p1a.get_EndImgFlag());
	write_header.template operator()<HeaderPart_2>(header_p1a.get_Part2Flag());
	write_header.template operator()<HeaderPart_3>(header_p1a.get_Part3Flag());
	write_header.template operator()<HeaderPart_4>(header_p1a.get_Part4Flag());

	return dst.first(dst.size() - current_header_area.size());
}

// if reset header parts functionality will be needed, implement check for 
// header part 4 presense: if part 4 is present, then all other headers must 
// be present as well except 1b.
//

namespace experimental {
	// experimental
	void override_headers() {
		// The concept of overriding the ccsds headers in already 
		// encoded/written segments is vague enough. 
		// First, some of the header data must be known before the segment can 
		// be encoded: 
		//		- all session parameters (because define dwt)
		//		- segment counter value (to access params via session context)
		//		- BitDepthDC and BitDepthAC (but guaranteed to be known via the 
		// 			segment data itself)
		//		- S aka segment size in blocks (but guaranteed to be known via
		//			the segment data itself)
		//		- stage/bitplane dependent truncation settings: (because can be 
		//			implemented by bpe encoders/decoders only)
		// 
		// Header data that is not necessary or may be not known by the time of 
		// bpe processing:
		//		- whether the segment is the last, and pad rows value
		//		- whether the segment is the first for the image session
		//		- whether the segment is the first for the application session
		//		- UseFill flag value (segment may be complemented up to 
		//			necessary amount of bytes at any time by writing the file)
		// 
		// 
		// Header parts that are already present in the segment's header group 
		// can be overriden under the following conditions after the header has		// TODO: grammar past simple
		// been already commited:
		//		- A segment part cannot be modified if corresponding 
		//			modification would be not valid for any consecutive segment 
		//			affected by the change. In addition to this requirement, 
		//			other rules for specific header parts apply.
		//		- p1a must be always present. Can mostly be altered in regards 
		//			to the following params: 
		//				- StartImgFlag
		//				- EndImgFlag
		//				- Part2Flag
		//				- Part3Flag
		//		- p1b may be modified freely with no preconditions. 
		//		- p2 may be modified in a limited manner if new setting does 
		//			not change the following values of the previously effective 
		//			setting: 
		//				- DCStop
		//				- BitPlaneStop
		//				- StageStop
		//			Having the conditions above satisfied, p2 may be changed 
		//			with the following constraints:
		//				- SegByteLimit value may be changed
		//					- to a value that is less than the previous 
		//						effective value with no precoditions
		//					- to a value that is greater than the previous 
		//						effective value if the real size of the segment 
		//						is less than the previous effective value (this 
		//						implies that UseFill flag value must be false)
		//				- UseFill value may be changed from false to true
		//		- p3 cannot be modified.
		//		- p4 cannot be modified.
		// Changing segment header parts must involve enforcement of the change 
		// on the current segment's data and all consecutive affected segments.
		// Implementation may expose 2 interfaces: for local and dependency 
		// fall-thru changes. Local change only requires the next segment to 
		// not be dependent on the changes (that means it has to contain the 
		// same segment part). Dependency fall-thru change requires header 
		// modification to be valid for all the dependent segments, and if so 
		// applies the change to all these segments.
		// For the purpose of dependency determination, a segment that has p1a 
		// EndImgFlag flag set to true (and p1b present accordingly) has no 
		// dependencies.
		//  
		// Header parts that are not present in the segment's header group can 
		// be added to the header group under the following conditions after 
		// the header has been already commited:
		//		- p1a is always present and cannot be added.
		//		- p1b can be added freely with no preconditions. Adding p1b 
		//			invalidates all consecutive segments up to a segment that 
		//			has p1a StartImgFlag flag set to true.
		//		- p2 can be added if it does not change the previously 
		//			effective values.
		//		- p3 can be added if it does not change the previously 
		//			effective values.
		//		- p4 cannot be added.
		// Addition of a segment part must involve setting corresponding p1a 
		// flag to true.
		// After a header part is added to the header group of the segment that 
		// was already commited previously, explicit size check and consecutive 
		// possible segment truncation must be performed.
		// 
		// Header parts that are already present in the segment's header group 
		// can be removed from the group under the following conditions after 
		// the header has been already commited:
		//		- A segment part may only be removed from the headers group if 
		//			sets of effective settings enforced by the segment part are 
		//			equal when the set of the previous segment compared to that 
		//			of the current segment, and the real size of the current 
		//			segment is less than effective p2 SegByteLimit value (that 
		//			implies effective p2 UseFill value must be false). In 
		//			addition to this requirement, other rules for specific 
		//			header parts apply.
		//		- No segment part can be removed if p4 is present.
		//		- p1a cannot be removed.
		//		- p1b cannot be removed once it is set.
		//		- p2 can be removed.
		//		- p3 can be removed.
		//		- p4 cannot be removed.
		// Removal of a segment part must involve setting corresponding p1a 
		// flag to false.
		// 
		// All segment header parts can be modified with no special limitations 
		// according to the protocol rules before the header group is commited.
		// Header part p4 can only be added before the segment's header group 
		// is commited; that implies that segment parts p2 and p3 are added as 
		// well. Later on, no segment part can be removed from the header 
		// group, and only p1b can be added. Defining p4 in the header group 
		// invalidates preceding segments up to the previous one that has p1a
		// EndImgFlag flag set to true (and p1b present accordingly).
		// To apply a change to the param that is defined by a header part to a
		// range of segments, the following steps should be taken:
		//		- in the next segment that should not carry effect of the 
		//			change, corresponding header part should be added, if not 
		//			present already.
		//		- in the first segment in the target range, corresponding 
		//			header part should be added, if not present already. 
		//		- attempt to incorporate the modification should be taken. At 
		//			this step, all dependent segments will be checked and 
		//			modified accordingly.
		//		- if the attempt failed, added headers should be removed, if 
		//			possible.
		// Adding p1b may make sense for the last segment only. Adding p3 may 
		// make sense for the first segment which has its effective settings 
		// set to the values described in the header, if it was not present in 
		// that segment's header previously.
		// 
		// //
	}
}
