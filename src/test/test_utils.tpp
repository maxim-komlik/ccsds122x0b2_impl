#pragma once

#include <random>

#include <dwt/bitmap.tpp>

template <typename T>
bitmap<T> generateNoisyBitmap(size_t width, size_t height, size_t offset,
	size_t seed = 1067, double phShift = 0.173f) {
	bitmap<T> input(width, height, offset);
	constexpr double pi = 3.141592653589793238;
	constexpr double stride = pi / (1 << 10);

	std::mt19937_64 generator(seed);
	std::uniform_real_distribution<> realdis(-5.0f, 5.0f);
	for (size_t i = 0; i < height; ++i) {
		double argument = phShift * i;
		for (size_t j = 0; j < width; ++j) {
			input[i][j] = (T)((realdis(generator) + cos(argument * 3) + sin(argument * 2)) * 1024 * 16);
			argument += stride;
		}
	}
	return input;
}
