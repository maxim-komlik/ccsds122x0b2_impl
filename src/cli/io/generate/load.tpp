#pragma once

#include <random>
#include <cmath>
#include <type_traits>

#include "dwt/bitmap.tpp"

#include "cli.hpp"

namespace cli::io::generate {

	template <typename T>
	inline bitmap<T> load_channel(size_t width, size_t height, size_t offset,
		size_t bdepth = ((sizeof(T) << 3) - 1 - 3),
		size_t seed = 1067, double phShift = 0.173f);


	// implementation section:

	template <typename T>
	inline bitmap<T> load_channel(size_t width, size_t height, size_t offset, size_t bdepth, size_t seed, double phShift) {
		bitmap<T> input(width, height, offset);
		constexpr double pi = 3.141592653589793238;
		constexpr double stride = pi / (1 << 10);
		float bdepth_exp = std::powf(2, (int)(bdepth)-3);		// or use std::ldexp?

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
