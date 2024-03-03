#pragma once

// TODO: This functionality is ptrovided by the standard library since C++23
// See std::byteswap
// THerefore should be removed.

#include <type_traits>

enum EndianOption {
	Little,
	Big
};

// TODO: this design may need refactoring. Current implementation is an 
// artifact left after the old template specialization impelementation.
template <typename T>
struct EndianCoder {
	typedef typename std::make_unsigned<T>::type uT;
	typedef typename std::make_signed<T>::type sT;
	static T apply(T word) {
		for (size_t i = 1; i < sizeof(T); i<<=1) {
			uT mask = ~((sT)(-1) << ((sizeof(T) / i / 2) * 8));
			for (size_t j = 1; j < i; ++j) {
				mask = (mask & ~((sT)(-1) << ((sizeof(T) / i) * 8))) ^ (mask << ((sizeof(T) / i) * 8));
			}
			word ^= (mask & word) << ((sizeof(T) / i / 2) * 8);
			word ^= (~mask & word) >> ((sizeof(T) / i / 2) * 8);
			word ^= (mask & word) << ((sizeof(T) / i / 2) * 8);
		}
		return word;
	}
};