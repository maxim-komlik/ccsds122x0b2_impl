#pragma once

#include <array>
#include <tuple>

#include "entropy_types.h"
#include "entropy_code.tpp"
#include "ibitwrapper.tpp"

struct EntropyTranslator {
private:
	std::tuple<
		// larger structs first
		std::tuple<entropy_code<4, 0>, entropy_code<4, 1>, entropy_code<4, 2>>,
		std::tuple<entropy_code<3, 0>, entropy_code<3, 1>>,
		std::tuple<entropy_code<2, 0>>> codes;

	std::array<std::array<symbol_encoding*, 3>, 5> mapping = { {
		{
			nullptr,
			nullptr,
			nullptr
		},
		{
			nullptr,
			nullptr,
			nullptr
		},
		{
			std::get<0>(std::get<2>(codes)).mapping,
			nullptr,
			nullptr
		},
		{
			std::get<0>(std::get<1>(codes)).mapping,
			std::get<1>(std::get<1>(codes)).mapping,
			nullptr
		},
		{
			std::get<0>(std::get<0>(codes)).mapping,
			std::get<1>(std::get<0>(codes)).mapping,
			std::get<2>(std::get<0>(codes)).mapping
		}
	} };

public:
	// nor copyable nor movable due to mapping member containing pointers
	EntropyTranslator() = default; // causes compiler not to define copy/move ctors implicitly
	EntropyTranslator& operator=(const EntropyTranslator& other) = delete;

	// check if needed
	size_t word_length(vlw_t& vlw, ptrdiff_t codeoption) {
		ptrdiff_t length = vlw.length;
		if (codeoption != -1) {
			length = this->mapping[length][codeoption][vlw.value].length;
		}
		return length;
	}

	size_t word_length(size_t code, size_t length, ptrdiff_t codeoption) {
		if (codeoption != -1) {
			length = this->mapping[length][codeoption][code].length;
		}
		return length;
	}

	vlw_t translate(vlw_t vlw, ptrdiff_t codeoption) {
		if (codeoption != -1) {
			vlw = this->mapping[vlw.length][codeoption][vlw.value];
		}
		return vlw;
	}

	// backward
	template <typename ibwT>
	vlw_t extract(ibitwrapper<ibwT>& input, size_t length, ptrdiff_t option) {
		// TODO: here should be extensive validation as the data source is unknown.
		// Raw pointers used for translations do not work properly here
		constexpr ptrdiff_t uncoded_option = -1;
		size_t value = 0;
		if (option == uncoded_option) {
			value = input.extract(length);
		}
		else {
			// Check entropy mapping entries one by one starting from the first. The length of
			// the consequtive entries increases. Try to match next until equal. Result value 
			// is index.
			// 
			// TODO: implement proper boundary checks. The source of input data is unknown. Throw 
			// exception on invalid input.
			symbol_encoding* table = this->mapping[length][option];
			vlw_t acc{ table[value].length, 0b0 };
			input >> acc; // handle the first entry explicitly to avoid -1 indexint param
			while (acc.value != table[value].value) { // yep, no boundary checks...
				++value;
				acc.length = table[value].length - table[value - 1].length;
				if (acc.length > 0) {
					input >> acc;
				}
			}
		}
		return { length, value };
	}
};
