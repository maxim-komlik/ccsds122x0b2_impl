#pragma once

#include <array>
#include <vector>

#include "dwt\bitmap.tpp"
#include "aligned_vector.tpp"

#include "constant.hpp"

// TODO: move subbands_t to dwt types header
template <typename T>
using subbands_t = std::array<bitmap<T>, constants::subband::subbands_per_img>;

using shifts_t = std::array<size_t, constants::subband::subbands_per_img>;

template <typename T>
struct block {
	T content[constants::block::items_per_block];
};

template <typename T>
struct segment {
	using type = T;

	size_t size;		// min value: 1
	size_t bdepthDc;	// min value: 1
	size_t bdepthAc; 	// min value: 0
	size_t q;			// min value: 0

	// Yet subband multiplication coefficients are not a property of 
	// a segment but rather a property of DWT application for all 
	// subbands of an entire image, subband weights are copied to 
	// every segment structure to weaken dependencies between
	// modules. This allows reusing BPE class for processing segments 
	// of different images in a single session with no need to 
	// perform image-specific setup.
	//
	shifts_t bit_shifts { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };

	T referenceSample;
	aligned_vector<T> quantizedDc;
	aligned_vector<T> plainDc;

	// but values are in range [0-63], 6 bits is enough...
	size_t referenceBdepthAc;
	aligned_vector<size_t> quantizedBdepthAc;

	std::vector<typename block<T>> data;
};

// // TODO: check utility below if needed/should be moved to another location
// constexpr size_t block_index_to_buf_index(size_t l) {
// 	size_t bound_i = std::bit_width(l);
// 	bound_i += bound_i & 0x01;
// 	ptrdiff_t level = bound_i >> 1;
// 	ptrdiff_t mask = (0b11 << bound_i) >> 2;
// 	ptrdiff_t disp = (l & mask) >> (relu(level - 1) << 1);
// 
// 	return (3 * relu(level - 1) + disp);
// }
