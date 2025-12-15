#pragma once

// TODO: This functionality is provided by the standard library since C++23
// See std::byteswap
// Therefore should be removed.
// 
// Still there's no reliable implementation of C++23 in the wild yet. Rolled
// back from the legacy.
//

#include <type_traits>

// TODO: unroll here it we take this implementation seriously
template <typename T>
T byteswap(T word) {
	typedef typename std::make_unsigned<T>::type uT;
	typedef typename std::make_signed<T>::type sT;
	for (size_t i = 1; i < sizeof(T); i <<= 1) {
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
