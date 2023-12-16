#pragma once

#include <type_traits>

enum EndianOption {
	Little,
	Big
};

template <typename T>
struct EndianCoder /*<Little, Big, T>*/ {
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