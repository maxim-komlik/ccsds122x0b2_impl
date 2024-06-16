#pragma once

#include <cstdint>

template <typename T = size_t>
struct _vlw_t {
	using type = T;

	T length;
	T value;

	template <typename D>
	operator _vlw_t<D>() {
		return _vlw_t<D>{ (D)(this->length), (D)(this->value) };
	};
};

using dense_vlw_t = _vlw_t<uint8_t>;
using vlw_t = _vlw_t<size_t>;
// 
// uint8_t is not proper type because symbol encodings have points 8 bit length.
// When coding to obitwrapper, shift operator for uint8_t and shift amount 8 bit 
// is UB.
using symbol_encoding = _vlw_t<uint_fast16_t>;


template <typename int_type>
struct char_type;

template <>
struct char_type <unsigned int> {
	typedef char32_t type;
};

template <>
struct char_type <unsigned short> {
	typedef char16_t type;
};
