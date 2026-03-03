#pragma once

#include <vector>
#include <memory>

#include "bitmap.tpp"
#include "core_types.hpp"
#include "compressor_types.hpp"
#include "utility.hpp"
#include "dwt/constant.hpp"


template <typename T, size_t alignment = 16>
class SegmentAssembler {
	using oT = compressor_type_params<T>::segment_type;
	shifts_t bit_shifts;
	size_t segment_size = 0;

public:
	using output_value_type = oT;
	using output_type = std::vector<std::unique_ptr<segment<output_value_type>>>;

public:
	output_type apply(subbands_t<T>&& buffers);

	shifts_t get_shifts() const;
	void set_shifts(const shifts_t& shifts);

	void set_segment_size(size_t S);
	size_t get_segment_size() const;
private:
	output_type pack(subbands_t<T>& buffers); // TODO: take by reference? const?
	inline oT handle_item_pack(T coeff) const;
};


template <typename T, size_t alignment = 16>
class SegmentDisassembler {
	using iT = compressor_type_params<T>::segment_type;
	size_t img_width = 0;

public:
	using input_value_type = iT;
	using input_type = std::vector<std::unique_ptr<segment<iT>>>;
	using output_type = std::vector<subbands_t<T>>;

	output_type apply(std::vector<std::unique_ptr<segment<iT>>>&& input);

	void set_image_width(size_t width);
	size_t get_image_width() const;
private:
	output_type unpack(std::vector<std::unique_ptr<segment<iT>>>&& input);
	inline T handle_item_unpack(iT coeff) const;
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
