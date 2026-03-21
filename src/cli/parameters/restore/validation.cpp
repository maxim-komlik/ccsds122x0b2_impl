#include "parameters/restore/validation.hpp"

#include <algorithm>
#include <type_traits>

#include "common/constant.hpp"
#include "dwt/constant.hpp"
#include "bpe/constant.hpp"
#include "dwt/utility.hpp"

#include "utility.hpp"


namespace cli::validate::restore {

namespace {

	validation_context validate_image_destomation(const params::destination& parameters);

}

validation_context validate_parameters(const params::restore_command& parameters) {
	validation_context result{ u8"restore"sv };

	result.nest_context(validate_image_destomation(parameters.dst_params));

	auto validate_stream_params = [&](const params::stream::parameters_set& item) {
		std::u8string id_str = to_u8string(item.id);
		std::u8string index_spec = u8"(see stream settings id="s + id_str + u8")";

		result.error(item.id >= 0,
			u8"Segment id must be nonnegative (id="s + id_str + u8" specified for stream settings). "s);
		result.error(item.byte_limit <= constants::bstream::out_byte_limit,
			u8"Input stream size limit must be not greater than 2^27 bytes "s + index_spec + u8". ");
		result.error(item.byte_limit >= 8,	// TODO: headers 1+3 
			u8"Input stream size limit must be not less than 8 bytes "s + index_spec + u8". ");
		result.warning(item.bplane_stop <= 31,		// TODO: implementation defined limit 
			u8"Input stream truncation bitplane index exceeds limit for standard conformant implementation (31); "s +
			u8"this may lead to image data loss "s + index_spec + u8". ");
		result.error(item.stage_stop <= 4,	// TODO: stages 0-4 
			u8"Input stream truncation stage index must be not greater than 4 "s + index_spec + u8". "s);
		result.error(item.stage_stop >= 0,	// TODO: stages 0-4 
			u8"Input stream truncation stage index must be not less than 0 "s + index_spec + u8". "s);
		};

	result.error(parameters.stream_params.first.id == 0,
		u8"Stream settings for id=0 are mandatory. "s);
	result.error(parameters.stream_params.first.byte_limit >= 19,	// TODO: headers 1+2+3+4
		u8"Input stream size limit must be not less than 19 bytes for segment id=0. "s);
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

	// TODO: validate source? validate destination?

	return result;
}


namespace {

	validation_context validate_image_destomation(const params::destination& parameters) {
		validation_context result{ u8"output_image"sv };

		switch (parameters.type) {
		case params::dst_type::file: {
			const auto& file_params = std::get<params::image_file>(parameters.parameters);
			std::u8string extension = file_params.path.extension().u8string();
			
			constexpr std::array known_extensions = std::invoke([]() constexpr {
					std::u8string_view values[] = {
						{u8".bmp"sv}
					};

					return std::to_array(values);
				});

			auto it = std::find_if(known_extensions.cbegin(), known_extensions.cend(),
				[&extension](auto item) -> bool { return item == extension; });

			result.error(!std::filesystem::exists(file_params.path), 
				u8"Output image file already exists. "s);
			result.error(it != known_extensions.cend(),
				u8"Output image format ["s + extension + u8"] is not supported. "s);
		}
		default: {

		}
		}

		return result;
	}

}

}
