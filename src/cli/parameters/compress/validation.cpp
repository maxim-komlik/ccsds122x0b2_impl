#include <algorithm>
#include <type_traits>

#include "common/constant.hpp"
#include "dwt/constant.hpp"
#include "bpe/constant.hpp"
#include "dwt/utility.hpp"

#include "parameters/compress/validation.hpp"
#include "utility.hpp"

namespace cli::validate::compress {

namespace {

	struct image_description {
		size_t width;
		size_t height;
		size_t channel_num;
		size_t static_bdepth;
		std::optional<bool> if_signed;
	};

	validation_context validate_image_source(const params::source& parameters);
	image_description get_image_description(const params::source& parameters);

}


// implementation section:

validation_context validate_parameters(const params::compress_command& parameters) {
	validation_context result{ u8"compress"sv };

	result.nest_context(validate_image_source(parameters.src_params));

	auto img_desc = get_image_description(parameters.src_params);

	if (parameters.img_transpose) {
		std::swap(img_desc.width, img_desc.height);
	}

	result.warning(img_desc.if_signed.value_or(parameters.img_signed) <= parameters.img_signed, 
		u8"Input image is signed, but DWT input configured as unsigned; this may result in unexpected dynamic bdepth values."s);

	result.error(img_desc.width >= constants::img::min_width,
		u8"Input image width must be not less than 17 (value "s + to_u8string(img_desc.width) + u8" provided). "s);
	result.error(img_desc.width <= constants::img::max_width,
		u8"Input image width must be not greater than 2^20 (value "s + to_u8string(img_desc.width) + u8" provided). "s);
	result.error(img_desc.height >= constants::img::min_height,
		u8"Input image height must be not less than 17 (value "s + to_u8string(img_desc.height) + u8" provided). "s);

	// standard conformance checks, but requirements may be loosen by implementation
	// generate warning on violation
	switch (parameters.dwt_params.type) {
	case params::dwt_type::integer: {
		result.warning(img_desc.static_bdepth <= constants::dwt::max_image_bdepth_int,
			u8"Input image bit depth exceeds the limit for integer DWT for standard conformant implementation (25); "s + 
			u8"this may lead to unexpected results. "s);
		break;
	}
	case params::dwt_type::fp: {
		result.warning(img_desc.static_bdepth <= constants::dwt::max_image_bdepth_fp_unsigned + parameters.img_signed, 
			u8"Input image bit depth exceeds the limit for floating point DWT for standard conformant implementation "s +
			u8"(27 [28 if signed pixel]); this may lead to unexpected results. "s);
		break;
	}
	default: {

	}
	}

	result.error((parameters.dwt_params.type != params::dwt_type::fp) & parameters.dwt_params.shifts.has_value(), 
		u8"Subband scale parameters (shifts) are valid for integer DWT only. "s);
	result.error((parameters.dwt_params.frame.width >= constants::img::min_width) | 
			(parameters.dwt_params.frame.width == 0),
		u8"DWT frame dimensions must be not less than 17 (width "s + 
			to_u8string(parameters.dwt_params.frame.width) + u8" provided). "s);
	result.error(((parameters.dwt_params.frame.width & constants::dwt::frame_dimension_granularity_mask) == 0), 
		u8"DWT frame dimensions must be multiple of 8 (width "s + 
			to_u8string(parameters.dwt_params.frame.width) + u8" provided). "s);
	result.error((parameters.dwt_params.frame.height >= constants::img::min_height) | 
			(parameters.dwt_params.frame.height == 0),
		u8"DWT frame dimensions must be not less than 17 (height "s + 
			to_u8string(parameters.dwt_params.frame.height) + u8" provided). "s);
	result.error(((parameters.dwt_params.frame.height & constants::dwt::frame_dimension_granularity_mask) == 0), 
		u8"DWT frame dimensions must be multiple of 8 (height "s + 
			to_u8string(parameters.dwt_params.frame.height) + u8" provided). "s);
	result.error(parameters.dwt_params.frame.width <= padded_image_dimensions(img_desc.width, img_desc.height).first, 
		u8"DWT frame width must not be greater than padded image width. "s);

	if (parameters.dwt_params.type == params::dwt_type::integer) {
		bool shifts_specified = parameters.dwt_params.shifts.has_value();
		result.warning(shifts_specified,
			u8"Subband scale shift values expected for integer DWT, but missing. "s);

		if (shifts_specified) {
			const auto& shift_values = parameters.dwt_params.shifts.value().values;
			for (ptrdiff_t i = 0; i < shift_values.size(); ++i) {
				std::u8string value_spec = u8"(value ["s + to_u8string(i + 1) + u8"]="s +
					to_u8string(shift_values[i]) + u8" provided)"s;
				result.error(shift_values[i] >= 0, 
					u8"DWT scale weight must be greater than 0"s + value_spec + u8". ");
				result.error(shift_values[i] <= constants::subband::max_scale_shift, 
					u8"DWT scale weight must be less than 2^3 (max shift value is 3) "s + value_spec + u8". ");
			}
		}
	}


	auto validate_segment_params = [&](const params::segment::parameters_set& item) {
		std::u8string id_str = to_u8string(item.id);
		std::u8string index_spec = u8"(see segment settings id="s + id_str + u8")";

		result.error(item.id >= 0,
			u8"Segment id must be nonnegative (id="s + id_str + u8" specified for segment settings). "s);
		result.error(item.size >= constants::segment::min_blocks_per_segment, 
			u8"Segment size must be not less than 16 " + index_spec + u8". "s);
		result.error(item.size <= constants::segment::max_blocks_per_segment, 
			u8"Segment size must be not greater than 2^20 "s + index_spec + u8". "s);
	};

	result.error(parameters.segment_params.first.id == 0, 
		u8"Segment settings for id=0 are mandatory. "s);
	validate_segment_params(parameters.segment_params.first);

	const auto& segment_params = parameters.segment_params.subsequent;
	bool segments_sequence_valid = std::is_sorted(segment_params.cbegin(), segment_params.cend(),
		[](const params::segment::parameters_set& lhs, const params::segment::parameters_set& rhs) -> bool {
			// here std::is_sorted is combined with std::adjacent_find == cend() by predicate.
			// The standard requires that predicate models Compare, providing value for logical 
			// "strict less than". Logical less than is redefined for the terget range and is 
			// equivalent to "not greater than"

			return !(lhs.id > rhs.id);
		});

	if (!segment_params.empty()) {
		result.error(segment_params.front().id != 0, 
			u8"Segment settings must be specified in ascending segment id order, no settings for the same id are allowed. "s);
	}
	result.error(segments_sequence_valid,
		u8"Segment settings must be specified in ascending segment id order, no settings for the same id are allowed. "s);

	std::for_each(segment_params.cbegin(), segment_params.cend(), validate_segment_params);


	auto validate_stream_params = [&](const params::stream::parameters_set& item) {
		std::u8string id_str = to_u8string(item.id);
		std::u8string index_spec = u8"(see stream settings id="s + id_str + u8")";

		result.error(item.id >= 0,
			u8"Segment id must be nonnegative (id="s + id_str + u8" specified for stream settings). "s);
		result.error(item.byte_limit <= constants::bstream::out_byte_limit, 
			u8"Output stream size limit must be not greater than 2^27 bytes "s + index_spec + u8". ");
		result.error(item.byte_limit >= 8,	// TODO: headers 1+3 
			u8"Output stream size limit must be not less than 8 bytes "s + index_spec + u8". ");
		result.warning(item.bplane_stop <= 31,		// TODO: implementation defined limit 
			u8"Output stream truncation bitplane index exceeds limit for standard conformant implementation (31); "s +
			u8"this may lead to image data loss "s + index_spec + u8". ");
		result.error(item.stage_stop <= 4,	// TODO: stages 0-4 
			u8"Output stream truncation stage index must be not greater than 4 "s + index_spec + u8". "s);
		result.error(item.stage_stop >= 0,	// TODO: stages 0-4 
			u8"Output stream truncation stage index must be not less than 0 "s + index_spec + u8". "s);

		// generate warning on violation
		result.warning(item.bplane_stop < img_desc.static_bdepth,
			u8"Output stream truncation bitplane index exceeds input image static bit depth "s + index_spec + u8". ");
	};

	result.error(parameters.stream_params.first.id == 0, 
		u8"Stream settings for id=0 are mandatory. "s);
	result.error(parameters.stream_params.first.byte_limit >= 19,	// TODO: headers 1+2+3+4
		u8"Output stream size limit must be not less than 19 bytes for segment id=0. "s); 
	validate_stream_params(parameters.stream_params.first);

	const auto& stream_params = parameters.stream_params.subsequent;
	bool stream_sequence_valid = std::is_sorted(stream_params.cbegin(), stream_params.cend(),
		[](const params::stream::parameters_set& lhs, const params::stream::parameters_set& rhs) -> bool {
			// here std::is_sorted is combined with std::adjacent_find == cend() by predicate.
			// The standard requires that predicate models Compare, providing value for logical 
			// "strict less than". Logical less than is redefined for the terget range and is 
			// equivalent to "not greater than"

			return !(lhs.id > rhs.id);
		});

	if (!stream_params.empty()) {
		result.error(stream_params.front().id != 0, 
			u8"Stream settings must be specified in ascending segment id order, no settings for the same id are allowed. "s);
	}
	result.error(stream_sequence_valid,
		u8"Stream settings must be specified in ascending segment id order, no settings for the same id are allowed. "s);

	std::for_each(stream_params.cbegin(), stream_params.cend(), validate_stream_params);

	return result;
}

namespace {

	validation_context validate_image_source(const params::source& parameters) {
		validation_context result{ u8"generate"sv };

		switch (parameters.type) {
		case params::src_type::generate: {
			using generator_arithm_t = double;

			const params::generate::generator& gen_params =
				std::get<params::generate::generator>(parameters.parameters);

			// checks below are covered by input image dimensions validation
			// result.error(gen_params.dims.width >= constants::img::min_width, 
			// 	u8"Generation target image width must be not less than 17. "s);
			// result.error(gen_params.dims.height >= constants::img::min_height, 
			// 	u8"Generation target image height must be not less than 17. "s);
			// result.error(gen_params.dims.width <= constants::img::max_width, 
			// 	u8"Generation target image width must be not greater than 2^20. "s);

			result.error(gen_params.dims.depth > 0, 
				u8"Generation target image depth must be nonnegative. "s);
			result.error(gen_params.bdepth <= std::numeric_limits<generator_arithm_t>::digits, 
				u8"Generation target image pixel bit depth must be not greater than "s +
				to_u8string(std::numeric_limits<generator_arithm_t>::digits));

			break;
		}
		default: {

		}
		}

		return result;
	}

	image_description get_image_description(const params::source& parameters) {
		switch (parameters.type) {
		case params::src_type::generate: {

			const params::generate::generator& gen_params =
				std::get<params::generate::generator>(parameters.parameters);

			return image_description{
				.width = gen_params.dims.width,
				.height = gen_params.dims.height,
				.channel_num = gen_params.dims.depth,
				.static_bdepth = gen_params.bdepth,
				.if_signed = gen_params.pixel_signed
			};

			break;
		}
		default: {

		}
		}

		// TODO: C++23 std::unreachable?
	}

}

}
