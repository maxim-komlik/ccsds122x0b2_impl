#include <vector>
#include <tuple>
#include <array>
#include <optional>		// see anonimous namespace
#include <utility>
#include <memory>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <type_traits>
#include <cstddef>

#include "dwt/bitmap.tpp"
#include "utils.hpp"

#include "common/constant.hpp"
#include "dwt/constant.hpp"
#include "bpe/constant.hpp"
#include "dwt/utility.hpp"

#include "io/io_settings.hpp"
#include "io/session_context.hpp"

#include "compress.hpp"
#include "restore.hpp"
#include "core_environment.tpp"
#include "file_utility.hpp"

namespace cli::command {

namespace compress {

namespace {

	storage_type parse_storage_type(params::dst_type value);
	dwt_type_t parse_dwt_type(params::dwt_type type);
	shifts_t parse_shifts(const std::optional<params::shifts>& value);
	std::pair<size_t, compression_settings> parse_compression_settings(const params::stream::parameters_set& value);
	std::pair<size_t, segment_settings> parse_segment_settings(const params::segment::parameters_set& value);

	// TODO: merge with validation implementation
	struct image_description {
		size_t width;
		size_t height;
		size_t channel_num;
		size_t static_bdepth;
		std::optional<bool> if_signed;
	};

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

namespace restore {

namespace {

	params::src_type;
	// {
	// 	file
	// };

	params::segment_protocol_type;
	// {
	// 	detect,
	// 	standard,
	// 	file,
	// 	memory
	// };

	params::dst_type;
	// {
	// 	memory
	// };

	params::image_protocol_type;	// raw

	storage_type parse_storage_type(params::src_type);

	std::pair<size_t, compression_settings> parse_compression_settings(const params::stream::parameters_set& value);

	std::vector<std::reference_wrapper<const data_descriptor>> load_segments(const params::source& src_params, 
		io_data_registry& registry);
}

}


// implementation section:

namespace compress {

void compress_command_handler(const params::compress_command& parameters) {
	// image data is anyway somehow compressed when stored, there's no reason to try to load one bitmap 
	// type and then cast it to bitmap with some extended underlying type to satisfy dwt requirements
	// 
	// so we need to parse bdepth and dwt type according to compressor_types, and then use size_t as 
	// codeword type
	//

	// parse dst first to create registry?
	// then set all session parameters
	// then parse session parameters to get type mapping
	// then load image data
	// then validate image data and validate dependent session parameters
	// then call target command implementation
	//

	parameters.dst_params.protocol; // TODO: add support for protocol selection
	parameters.dst_params.parameters; // TODO:

	io_data_registry registry(parse_storage_type(parameters.dst_params.type));

	image_description img_desc = get_image_description(parameters.src_params);
	size_t height_padding = padded_image_dimensions(img_desc.width, img_desc.height).second - img_desc.height;

	parameters.dwt_params.frame;	// TODO:

	session_context cx(registry);
	cx.settings_session = session_settings {
		.dwt_type = parse_dwt_type(parameters.dwt_params.type),
		.img_width = img_desc.width,
		.pixel_bdepth = img_desc.static_bdepth, // TODO:
		.signed_pixel = parameters.img_signed,
		.transpose = parameters.img_transpose,
		.rows_pad_count = height_padding,
		.codeword_size = sizeof(uintptr_t) << 3,	// TODO: ?
		.custom_shifts = parameters.dwt_params.shifts.has_value(),
		.shifts = parse_shifts(parameters.dwt_params.shifts)
	};

	cx.compr_settings.push_back(parse_compression_settings(parameters.stream_params.first));
	for (const auto& item : parameters.stream_params.subsequent) {
		cx.compr_settings.push_back(parse_compression_settings(item));
	}

	cx.seg_settings.push_back(parse_segment_settings(parameters.segment_params.first));
	for (const auto& item : parameters.segment_params.subsequent) {
		cx.seg_settings.push_back(parse_segment_settings(item));
	}

	session_parameters_parser<flow_impl>::load_image(std::move(cx), img_desc.channel_num, parameters.src_params);

	// TODO: handle output data somehow?
}

namespace {

	storage_type parse_storage_type(params::dst_type value) {
		constexpr std::pair<params::dst_type, storage_type> map_content[]{
			{ params::dst_type::file, storage_type::file },
			{ params::dst_type::memory, storage_type::memory }
		};
		constexpr std::array storage_mapping = std::to_array(map_content);

		auto storage_it = std::find_if(storage_mapping.cbegin(), storage_mapping.cend(),
			[&](const auto& pair) -> bool {
				return pair.first == value;
			});

		if (storage_it == storage_mapping.cend()) {
			// TODO: throw: not supported
		}

		return storage_it->second;
	}

	dwt_type_t parse_dwt_type(params::dwt_type type) {
		constexpr std::pair<params::dwt_type, dwt_type_t> map_content[] = {
			{params::dwt_type::integer, dwt_type_t::idwt},
			{params::dwt_type::fp, dwt_type_t::fdwt}
		};
		constexpr std::array storage_mapping = std::to_array(map_content);

		auto storage_it = std::find_if(storage_mapping.cbegin(), storage_mapping.cend(),
			[&](const auto& pair) -> bool {
				return pair.first == type;
			});

		if (storage_it == storage_mapping.cend()) {
			// TODO: throw: not supported
		}

		return storage_it->second;
	}

	shifts_t parse_shifts(const std::optional<params::shifts>& value) {
		if (value) {
			return value->values;
		}
		return { 0 };
	}

	std::pair<size_t, compression_settings> parse_compression_settings(const params::stream::parameters_set& value) {
		bool bpe_truncated = false;
		// TODO: refactor early stream termination flag. It happens to be used for byte limit by 
		// protocol, but shouldn't. Likely member function aggregator needed

		return {
			value.id,
			{
				.seg_byte_limit = value.byte_limit,
				.use_fill = value.fill,
				.early_termination = bpe_truncated,
				.DC_stop = value.DC_stop,
				.bplane_stop = value.bplane_stop,
				.stage_stop = value.stage_stop
			}
		};
	}

	std::pair<size_t, segment_settings> parse_segment_settings(const params::segment::parameters_set& value) {
		return {
			value.id,
			{
				.size = value.size,
				.heuristic_quant_DC = value.heuristic_DC,
				.heuristic_bdepth_AC = value.heuristic_AC_bdepth
			}
		};
	}
}

}

namespace restore {

void restore_command_handler(const params::restore_command& parameters) {
	std::vector<std::pair<size_t, compression_settings>> override_compr_params;
	override_compr_params.push_back(parse_compression_settings(parameters.stream_params.first));
	for (const auto& item : parameters.stream_params.subsequent) {
		override_compr_params.push_back(parse_compression_settings(item));
	}
	// TODO: overriding encoded compression settings is not supported yet for image restore command

	io_data_registry registry(parse_storage_type(parameters.src_params.type));
	
	session_context cx(registry);

	auto handles = load_segments(parameters.src_params, registry);
	size_t channel_num = collect_decompression_session_params(cx, handles);
	session_parameters_parser<flow_impl>::restore(std::move(cx), channel_num, std::move(handles));
}

namespace {

	storage_type parse_storage_type(params::src_type value) {
		constexpr std::pair<params::src_type, storage_type> map_content[]{
			{ params::src_type::file, storage_type::file }
		};
		constexpr std::array storage_mapping = std::to_array(map_content);

		auto storage_it = std::find_if(storage_mapping.cbegin(), storage_mapping.cend(),
			[&](const auto& pair) -> bool {
				return pair.first == value;
			});

		if (storage_it == storage_mapping.cend()) {
			// TODO: throw: not supported
		}

		return storage_it->second;
	}

	std::pair<size_t, compression_settings> parse_compression_settings(const params::stream::parameters_set& value) {
		bool bpe_truncated = false;
		// TODO: refactor early stream termination flag. It happens to be used for byte limit by 
		// protocol, but shouldn't. Likely member function aggregator needed

		return {
			value.id,
			{
				.seg_byte_limit = value.byte_limit,
				.use_fill = false,
				.early_termination = bpe_truncated,
				.DC_stop = value.DC_stop,
				.bplane_stop = value.bplane_stop,
				.stage_stop = value.stage_stop
			}
		};
	}

	std::vector<std::reference_wrapper<const data_descriptor>> load_segments(const params::source& parameters,
			io_data_registry& registry) {
		std::vector<std::reference_wrapper<const data_descriptor>> result;

		switch (parameters.type) {
		case params::src_type::file: {
			// TODO: catch std::bad_variant_access?
			const params::file_sink_params& file_params = std::get<params::file_sink_params>(parameters.parameters);

			std::vector<std::filesystem::path> segment_paths;
			if (if_path_is_pattern(file_params.path)) {
				segment_paths = expand_pattern(file_params.path);
			} else {
				if (std::filesystem::exists(file_params.path)) {
					segment_paths.push_back(file_params.path);
				}
			}

			if (segment_paths.empty()) {
				// TODO: throw, invalid input
			}

			std::sort(segment_paths.begin(), segment_paths.end(),
				[](const auto& lhs, const auto& rhs) -> bool {
					// lifetime extended for return objects
					const std::u8string& lhs_stem = lhs.stem().u8string();
					const std::u8string& rhs_stem = rhs.stem().u8string();
					// TODO: lexicographical-like comparison is reasonbly poor approach for utf strings, is it?

					// return std::lexicographical_compare(lhs_stem.cbegin(), lhs_stem.cend(), 
					// 	rhs_stem.cbegin(), rhs_stem.cend());

					auto mismatch = std::mismatch(lhs_stem.cbegin(), lhs_stem.cend(),
						rhs_stem.cbegin(), rhs_stem.cend());
					
					auto if_ascii_digit = [](char8_t item) -> bool {
						// put in a vector of 16, pad with \0? unroll with unordered equal comparison? aggregate or?
						constexpr std::array digits = {
							u8'0', u8'1', u8'2', u8'3', u8'4', u8'5', u8'6', u8'7', u8'8', u8'9'
						};
						return std::find(digits.cbegin(), digits.cend(), item) != digits.cend();
					};

					std::pair digits_end = {
						std::find_if_not(mismatch.first, lhs_stem.cend(), if_ascii_digit), 
						std::find_if_not(mismatch.second, rhs_stem.cend(), if_ascii_digit)
					};

					std::pair digits_count = {
						std::distance(mismatch.first, digits_end.first),
						std::distance(mismatch.second, digits_end.second)
					};

					bool lhs_is_less = false;
					// lhs_is_less &= (mismatch.second != rhs_stem.cend());
					// lhs_is_less |= (digits_count.second != 0);

					lhs_is_less |= (digits_count.first < digits_count.second);

					if (!lhs_is_less) {
						lhs_is_less |= 
							(
								(digits_count.first == digits_count.second) & 
								(digits_count.second > 0)
							) && (*mismatch.first < *mismatch.second);
					}

					return lhs_is_less;
				});


			for (ptrdiff_t i = 0; i < segment_paths.size(); ++i) {
				result.push_back(registry.put_input(
					segment_file_descriptor(std::move(segment_paths[i]), 0, i)));
			}

			break;
		}
		default: {

		}
		}

		// TODO: C++23 std::unreachable?
		return result;
	}

}

}

}
