#pragma once

#include <algorithm>
#include <random>
#include <cmath>
#include <utility>
#include <type_traits>

#include "dwt/bitmap.tpp"

#include "common/constant.hpp"
#include "dwt/constant.hpp"
#include "bpe/constant.hpp"
#include "dwt/utility.hpp"

#include "cli.hpp"
#include "parameters/compress/parameters.hpp"

namespace cli::command::compress {

namespace params = cli::parameters::compress;

template <typename T>
inline bitmap<T> load_image_channel(const params::source& parameters, size_t channel_index);

template <typename T>
inline bitmap<T> generate_image(size_t width, size_t height, size_t offset,
	size_t bdepth = ((sizeof(T) << 3) - 1 - 3),
	size_t seed = 1067, double phShift = 0.173f);

	
// implementation section:

template <typename T>
inline bitmap<T> load_image_channel(const cli::parameters::compress::source& parameters, size_t channel_index) {

	auto compute_offset = [](size_t width) -> size_t {
		// TODO: review
		if (width < 64) {
			return 32;
		}
		else {
			return 16;
		}
		};

	switch (parameters.type) {
	case params::src_type::generate: {

		const params::generate::generator& gen_params =
			std::get<params::generate::generator>(parameters.parameters);

		bool valid = true;
		valid &= channel_index < gen_params.dims.depth;
		valid &= gen_params.bdepth <= std::numeric_limits<T>::digits;
		valid &= gen_params.pixel_signed != std::is_signed_v<T>;
		if (!valid) {
			// TODO: error handling, throw
		}

		return generate_image<T>(gen_params.dims.width, gen_params.dims.height,
			compute_offset(gen_params.dims.width), gen_params.bdepth, gen_params.generator_seed);
		break;
	}
	default: {

	}
	}

}

template <typename T>
inline bitmap<T> generate_image(size_t width, size_t height, size_t offset, size_t bdepth, size_t seed, double phShift) {
	bitmap<T> input(width, height, offset);
	constexpr double pi = 3.141592653589793238;
	constexpr double stride = pi / (1 << 10);
	float bdepth_exp = std::powf(2, (int)(bdepth) - 3);		// or use std::ldexp?

	std::mt19937_64 generator(seed);
	std::uniform_real_distribution<> realdis(-4.0f, 4.0f);
	for (size_t i = 0; i < height; ++i) {
		double argument = phShift * i;
		for (size_t j = 0; j < width; ++j) {
			// value is roughly *somehow* distributed across [-8, +8]
			// the purpose of cos/sin is to add correlation into the data, potentially exploited by dwt
			double value = realdis(generator) + 
				(std::cos(argument * 3) * 2) + 
				(std::sin(argument * 2) * 2);

			if (std::is_unsigned_v<T>) {
				value = std::abs(value);
			}

			input[i][j] = static_cast<T>(value * bdepth_exp);
			argument += stride;
		}
	}

	return input;
}

}
