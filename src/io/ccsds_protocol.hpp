#pragma once

#include <array>
#include <utility>

#include "core_types.hpp"
#include "utils.hpp"

#include "io_settings.hpp"
#include "headers.hpp"

class ccsds_protocol {
public:
	template <typename T>
	ccsds_protocol(const segment<T>& data, size_t segment_index);

	void init_session(const session_settings& session_params, 
			const segment_settings& segment_params, const compression_settings& compression_params);
	void set_segment_params(const segment_settings& params);
	void set_compression_params(const compression_settings& params);
	void set_first(bool value);
	void set_last(size_t padding);


	bool if_first() const;
	bool if_last() const;


	size_t header_size() const;
	// virtual void commit() = 0;

protected:
	virtual bool if_headers_overridable() const { return false; }

	uint8_t* write_headers(uint8_t* dst);

private:
	template <typename T>
	using header_record_t = std::tuple<T, bool>;
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
	header_p3.set_OptDCSelect(params.heuristic_quant_DC);
	header_p3.set_OptACSelect(params.heiristic_bdepth_AC);

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

size_t ccsds_protocol::header_size() const {
	size_t result = 0;
	const auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	result += header_p1a.size();

	if (header_p1a.get_EndImgFlag()) {
		const auto& [header_p1b, flag_p1b] = std::get<header_record_t<HeaderPart_1B>>(this->headers);
		result += header_p1b.size();
	}
	if (header_p1a.get_Part2Flag()) {
		const auto& [header_p2, flag_p2] = std::get<header_record_t<HeaderPart_2>>(this->headers);
		result += header_p2.size();
	}
	if (header_p1a.get_Part3Flag()) {
		const auto& [header_p3, flag_p3] = std::get<header_record_t<HeaderPart_3>>(this->headers);
		result += header_p3.size();
	}
	if (header_p1a.get_Part4Flag()) {
		const auto& [header_p4, flag_p4] = std::get<header_record_t<HeaderPart_4>>(this->headers);
		result += header_p4.size();
	}

	return result;
}

uint8_t* ccsds_protocol::write_headers(uint8_t* dst) {
	if (this->if_headers_overridable()) {

	}

	auto& [header_p1a, flag_p1a] = std::get<header_record_t<HeaderPart_1A>>(this->headers);
	const auto& content_p1a = header_p1a.commit();
	dst = std::copy_n(content_p1a.data(), content_p1a.size(), dst);

	if (header_p1a.get_EndImgFlag()) {
		auto& [header_p1b, flag_p1b] = std::get<header_record_t<HeaderPart_1B>>(this->headers);
		const auto& content_p1b = header_p1b.commit();
		dst = std::copy_n(content_p1b.data(), content_p1b.size(), dst);
	}
	if (header_p1a.get_Part2Flag()) {
		auto& [header_p2, flag_p2] = std::get<header_record_t<HeaderPart_2>>(this->headers);
		const auto& content_p2 = header_p2.commit();
		dst = std::copy_n(content_p2.data(), content_p2.size(), dst);
	}
	if (header_p1a.get_Part3Flag()) {
		auto& [header_p3, flag_p3] = std::get<header_record_t<HeaderPart_3>>(this->headers);
		const auto& content_p3 = header_p3.commit();
		dst = std::copy_n(content_p3.data(), content_p3.size(), dst);
	}
	if (header_p1a.get_Part4Flag()) {
		auto& [header_p4, flag_p4] = std::get<header_record_t<HeaderPart_4>>(this->headers);
		const auto& content_p4 = header_p4.commit();
		dst = std::copy_n(content_p4.data(), content_p4.size(), dst);
	}

	return dst;
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
