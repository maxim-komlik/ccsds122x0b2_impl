#pragma once

#include "aligned_vector.tpp"
#include <array>
#include <vector>

template <typename T>
union block {
	T content[64];
};

template <typename T>
struct segment {
	size_t size;
	size_t bdepthDc;	// min value: 1
	size_t bdepthAc; 	// min value: 0
	size_t q;

	// TODO: implement custom weights during DWT pack
	std::array<size_t, 10> bit_shifts { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };

	T referenceSample;
	aligned_vector<T> quantizedDc;
	aligned_vector<T> plainDc;

	// but values are in range [0-63], 6 bits is enough...
	size_t referenceBdepthAc;
	// aligned_vector<size_t> bdepthAcBlocks;
	aligned_vector<size_t> quantizedBdepthAc;

	std::vector<typename block<T>> data;
};
