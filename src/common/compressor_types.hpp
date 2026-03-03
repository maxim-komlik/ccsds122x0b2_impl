#pragma once

#include <type_traits>
#include <array>

#include "utility.hpp"
#include "constant.hpp"

template <typename dwtT>
struct compressor_type_params {
private:
	template <typename NonIntT, bool is_integral>
	struct make_dwt_type_impl {
		using type = NonIntT;
	};

	template <typename IntT>
	struct make_dwt_type_impl<IntT, true> {
		using type = IntT;
	};

	template <typename T>
	using make_dwt_type = make_dwt_type_impl<T, std::is_integral_v<T>>::type;

public:
	// underlying dwt type has to be signed. std::make_signed is defined for integrals only
	using dwt_type = make_dwt_type<dwtT>;
	using subband_type = dwt_type;
	using block_type = sufficient_integral<dwt_type>;
	using segment_type = block_type;
	using bpe_type = segment_type;
};


template <dwt_type_t dwt_type, size_t bdepth, bool img_is_signed = true>
struct img_type_params;

// integer dwt, signed input
//

template <>
struct img_type_params<dwt_type_t::idwt, 4> {
	using bitmap_type = int8_t;
	using dwt_param_type = int8_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 7> {
	using bitmap_type = int8_t;
	using dwt_param_type = int16_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 12> {
	using bitmap_type = int16_t;
	using dwt_param_type = int16_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 15> {
	using bitmap_type = int16_t;
	using dwt_param_type = int32_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 28> {
	using bitmap_type = int32_t;
	using dwt_param_type = int32_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 31> {
	using bitmap_type = int32_t;
	using dwt_param_type = int64_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 60> {
	using bitmap_type = int64_t;
	using dwt_param_type = int64_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};


//
// integer dwt, unsigned input

template <>
struct img_type_params<dwt_type_t::idwt, 4, false> {
	using bitmap_type = uint8_t;
	using dwt_param_type = int8_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 8, false> {
	using bitmap_type = uint8_t;
	using dwt_param_type = int16_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 12, false> {
	using bitmap_type = uint16_t;
	using dwt_param_type = int16_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 16, false> {
	using bitmap_type = uint16_t;
	using dwt_param_type = int32_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 28, false> {
	using bitmap_type = uint32_t;
	using dwt_param_type = int32_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 32, false> {
	using bitmap_type = uint32_t;
	using dwt_param_type = int64_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::idwt, 60, false> {
	using bitmap_type = uint64_t;
	using dwt_param_type = int64_t;
	using compressor_types = compressor_type_params<dwt_param_type>;
};


//
// floating point dwt, signed input
// 
// Until c++23 is generally available, there's no alternative for fixed width fp16.
//

template <>
struct img_type_params<dwt_type_t::fdwt, 7> {
	using bitmap_type = int8_t;
	using dwt_param_type = float;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::fdwt, 15> {
	using bitmap_type = int16_t;
	using dwt_param_type = float;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::fdwt, 21> {
	using bitmap_type = int32_t;
	using dwt_param_type = float;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::fdwt, 31> {
	using bitmap_type = int32_t;
	using dwt_param_type = double;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::fdwt, 50> {
	using bitmap_type = int64_t;
	using dwt_param_type = double;
	using compressor_types = compressor_type_params<dwt_param_type>;
};


//
// floating point dwt, unsigned input

template <>
struct img_type_params<dwt_type_t::fdwt, 8, false> {
	using bitmap_type = uint8_t;
	using dwt_param_type = float;
	using compressor_types = compressor_type_params<dwt_param_type>;
};
template <>
struct img_type_params<dwt_type_t::fdwt, 16, false> {
	using bitmap_type = uint16_t;
	using dwt_param_type = float;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::fdwt, 21, false> {
	using bitmap_type = uint32_t;
	using dwt_param_type = float;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::fdwt, 32, false> {
	using bitmap_type = uint32_t;
	using dwt_param_type = double;
	using compressor_types = compressor_type_params<dwt_param_type>;
};

template <>
struct img_type_params<dwt_type_t::fdwt, 50, false> {
	using bitmap_type = uint64_t;
	using dwt_param_type = double;
	using compressor_types = compressor_type_params<dwt_param_type>;
};


struct bdepth_edge_values {
	static constexpr std::array<size_t, 7> idwt_signed_input = { 4, 7, 12, 15, 28, 31, 60 };
	static constexpr std::array<size_t, 7> idwt_unsigned_input = { 4, 8, 12, 16, 28, 32, 60 };
	static constexpr std::array<size_t, 5> fdwt_signed_input = { 7, 15, 21, 31, 50 };
	static constexpr std::array<size_t, 5> fdwt_unsigned_input = { 8, 16, 21, 32, 50 };
};


template <dwt_type_t dwt_type, bool is_signed>
struct bdepth_implementation_catalog;

template <>
struct bdepth_implementation_catalog<dwt_type_t::idwt, true> {
	static constexpr std::array<size_t, 7> values = { 4, 7, 12, 15, 28, 31, 60 };
};

template <>
struct bdepth_implementation_catalog<dwt_type_t::idwt, false> {
	static constexpr std::array<size_t, 7> values = { 4, 8, 12, 16, 28, 32, 60 };
};

template <>
struct bdepth_implementation_catalog<dwt_type_t::fdwt, true> {
	static constexpr std::array<size_t, 5> values = { 7, 15, 21, 31, 50 };
};

template <>
struct bdepth_implementation_catalog<dwt_type_t::fdwt, false> {
	static constexpr std::array<size_t, 5> values = { 8, 16, 21, 32, 50 };
};


// Implementation notes:
// 
// The standard limits input data bit depth to be representable by 32-bit 
// signed integer after DWT and applicable post-processing (scaling for 
// integers and ceiling for floating point) applied. Segment processing 
// classes take the type defined by DWT processing classes as a template
// parameter, but the type of elements of a segment can be different 
// (integer for floating point input), that makes it necessary to compute 
// output type based on input type (passed as a template parameter). For 
// integer input data type, output data type can be the same (signed with 
// the same bit length). When DWT is applied to floating-point input, the 
// data type of DWT should be computed in a way so that mantiss of the 
// result type can hold the values of possibly increased bitness of whole 
// part; e.g. for image with bdepth == 24, 32-bit IEEE754-compatible
// floating point cannot be used as an output type because its 23-bit 
// mantiss cannot represent possibly increased in depth (up to 28 bit 
// + 1 bit for sign) whole part of a fraction. It should be noted that 
// the output of segment processing and subsequent BPE is the same for 
// any integral data type capable to represent the output of DWT; e.g. 
// the result of processing segments with 32-bit integers and segments 
// with 64-bit integers are the same (having valid bdepth parameters).
// Therefore an integer type of the same bitsize as the input floating
// point type can be used for segment and BPE processing, producing 
// valid result. Though segment and BPE processing implementation with 
// narrow integer type may benefit from decreased memory consumption 
// and increased data spatial locality.
// 
// For integer DWT, the same data type can be used to represent image 
// pixel, DWT coefficient on any level, and segment data item (up to 
// 32-bit integer).
// 
// For floating-point DWT, below is a table for every input bdepth:
// 
// +--------+----------+--------+---------+-------+
// | bdepth |  bitmap  | dwt(f) | segment |  bpe  |
// +--------+----------+--------+---------+-------+
// | 1*     | (u)i8	   |  f8    |   i8    |  i8   |
// +--------+----------+--------+---------+-------+
// | 1-7    | (u)i8	   |  f16   |   i16   |  i16  |
// +--------+----------+--------+---------+-------+
// | 8      | ui8/i16  |  f16   |   i16   |  i16  |
// +--------+----------+--------+---------+-------+
// | 9-15   | (u)i16   |  f32   |   i32   |  i32  |
// +--------+----------+--------+---------+-------+
// | 16     | ui16/i32 |  f32   |   i32   |  i32  |
// +--------+----------+--------+---------+-------+
// | 17-21  | (u)i32   |  f32   |   i32   |  i32  |
// +--------+----------+--------+---------+-------+
// | 22-28  | (u)i32   |  f64   |   i64   |  i64  |
// +--------+----------+--------+---------+-------+
//  Below is not compliant with ccsds122x0b2 (2017)
// +--------+----------+--------+---------+-------+
// | 29-31  | (u)i32   |  f64   |   i64   |  i64  |
// +--------+----------+--------+---------+-------+
// | 32     | ui32/i64 |  f64   |   i64   |  i64  |
// +--------+----------+--------+---------+-------+
// | 33-50  | (u)i64   |  f64   |   i64   |  i64  |
// +--------+----------+--------+---------+-------+
// 
// * for input with bdepth == 1 use of f8 (e4m3) is technically 
// possible.
//
