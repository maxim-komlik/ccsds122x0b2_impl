#pragma once

#include <array>
#include <vector>

#include ".\..\WaveletTransform\bitmap.tpp"  // TODO: refactor
#include "aligned_vector.tpp"

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

	// Yet subband multiplication coefficients are not a property of 
	// a segment but rather a property of DWT application for all 
	// subbands of an entire image, subband weights are copied to 
	// every segment structure to weaken dependencies between
	// modules. This allows reusing BPE class for processing segments 
	// of different images in a single session with no need to 
	// perform image-specific setup.
	//
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

constexpr static size_t subband_num = 10;

template <typename T>
using subbands_t = std::array<bitmap<T>, subband_num>;

using shifts_t = std::array<size_t, subband_num>;
