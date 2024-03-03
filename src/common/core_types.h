#pragma once

#include "alligned_vector.tpp"
#include <vector>

template <typename T>
union block {
	T content[64];
};

template <typename T>
struct segment {
	size_t size;
	size_t bdepthDc;
	size_t bdepthAc;
	size_t q;

	T referenceSample;
	alligned_vector<T> quantizedDc;
	alligned_vector<T> plainDc;

	std::vector<typename block<T>> data;
};
