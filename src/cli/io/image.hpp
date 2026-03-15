#pragma once

#include <optional>
#include <cstddef>

#include "dwt/bitmap.tpp"

#include "cli.hpp"
#include "parameters/compress/parameters.hpp"
#include "file_cache.hpp"
#include "generate/load.tpp"
#include "bmp/load.tpp"

namespace cli::io {

struct image_description {
	size_t width;
	size_t height;
	size_t channel_num;
	size_t static_bdepth;
	std::optional<bool> if_signed;
};

struct image_load_parameters {
	parameters::compress::source cli_parameters;
	image_description meta;
};

image_load_parameters get_image_description(const parameters::compress::source& parameters);
image_load_parameters get_image_description(parameters::compress::source&& parameters);


template <typename T>
inline bitmap<T> load_image_channel(const image_load_parameters& parameters, size_t channel_index);

}


// implementation section:

namespace cli::io {

template <typename T>
inline bitmap<T> load_image_channel(const image_load_parameters& parameters, size_t channel_index) {
	namespace params = parameters::compress;

	size_t image_row_offset = std::invoke([&]() -> size_t {
			// TODO: review
			if (parameters.meta.width < 64) {
				return 32;
			} else {
				return 16;
			}
		});

	switch (parameters.cli_parameters.type) {
	case params::src_type::generate: {
		const auto& gen_params = std::get<params::generate::generator>(parameters.cli_parameters.parameters);

		bool valid = true;
		valid &= channel_index < gen_params.dims.depth;
		valid &= gen_params.bdepth <= std::numeric_limits<T>::digits;
		valid &= gen_params.pixel_signed != std::is_signed_v<T>;
		if (!valid) {
			// TODO: error handling, throw
		}

		return generate::load_channel<T>(gen_params.dims.width, gen_params.dims.height,
			image_row_offset, gen_params.bdepth, gen_params.generator_seed);
		break;
	}
	case params::src_type::file: {
		const auto& file_params = std::get<params::image_file>(parameters.cli_parameters.parameters);
		std::u8string extension = file_params.path.extension().u8string();

		constexpr std::array known_extensions = std::invoke([]() constexpr {
				using namespace std::literals;

				using handler_t = bitmap<T>(*)(const file_descriptor&, ptrdiff_t, size_t);

				std::pair<std::u8string_view, handler_t> values[] = {
					{u8".bmp"sv, bmp::load_channel<T>}
				};
				return std::to_array(values);
			});

		auto it = std::find_if(known_extensions.cbegin(), known_extensions.cend(),
			[&extension](const auto& item) -> bool { return item.first == extension; });

		bool valid = true;
		valid &= (it != known_extensions.cend());

		if (!valid) {
			// TODO: error handling
		}

		const file_descriptor& descriptor = file_cache_instance.get_descriptor(file_params.path);

		return std::invoke(it->second, descriptor, channel_index, image_row_offset);
	}
	default: {

	}
	}

}

}
