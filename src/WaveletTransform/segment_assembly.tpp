#pragma once

#include <array>
#include <vector>
#include <bit>

#include "core_types.h"
#include "utils.h"
#include "bitmap.tpp"

template <typename T, size_t alignment = 16>
class SegmentAssembler {
	using oT = sufficient_integral<T>;
	subbands_t<T> buffers;
	shifts_t bit_shifts;
	size_t segment_size = 0; // or passed as a parameter to apply?

	static constexpr size_t c_families_count = 3;
	static constexpr size_t c_block_item_count = 64;
public:
	SegmentAssembler(subbands_t<T> input);
	std::vector<segment<oT>> apply();

	shifts_t get_shifts() const;
	void set_shifts(const shifts_t& shifts);
private:
	std::vector<segment<oT>> pack();
	inline oT handle_item_pack(T coeff) const;
};

template <typename T, size_t alignment = 16>
class SegmentDisassembler {
	using iT = sufficient_integral<T>;
	std::vector<typename segment<T>> segments;

	static constexpr size_t c_families_count = 3;
public:
	typedef std::vector<subbands_t<T>> output_t;

	SegmentDisassembler(std::vector<segment<iT>> input);
	output_t apply(size_t image_width);
private:
	output_t unpack(size_t image_width);
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
// matiss cannot represent possibly increased in depth (up to 28 bit 
// + 1 bit for sign) whole part of a fraction. It should be noted that 
// the output of segment processing and subsequent BPE is the same for 
// any integral data type capable to represent the output of DWT; e.g. 
// the result of processing segments with 32-bit integers and segments 
// with 64-bit integers are the same (having valide bdepth parameters).
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


// SegmentAssembler class template implementation

template <typename T, size_t alignment>
SegmentAssembler<T, alignment>::SegmentAssembler(subbands_t<T> input) : buffers(input), segment_size(0) {}

template <typename T, size_t alignment>
std::vector<segment<typename SegmentAssembler<T, alignment>::oT>> SegmentAssembler<T, alignment>::pack() {
	// Does not pack DC coefficients into blocks, AC only

	img_meta DC_loc = this->buffers[0].get_meta();
	size_t block_count = (DC_loc.width * DC_loc.height);
	if (this->segment_size == 0) {
		this->segment_size = block_count;
	}
	size_t segment_count = (block_count + (this->segment_size - 1)) / this->segment_size;
	std::vector<segment<oT>> output(segment_count); // implicitly calls default ctor for vector, size = 0
	--segment_count;

	size_t segment_index = 0, block_index = 0;
	output[segment_index].size = segment_index < segment_count ? this->segment_size : block_count;
	output[segment_index].bit_shifts = this->bit_shifts;
	output[segment_index].data.assign(output[segment_index].size, block<oT>());
	for (size_t i = 0; i < DC_loc.height; ++i) {
		for (size_t j = 0; j < DC_loc.width; ++j) {
			if (block_index >= this->segment_size) {
				block_index = 0;
				++segment_index;
				output[segment_index].size = segment_index < segment_count ?
					this->segment_size : block_count % this->segment_size;
				output[segment_index].bit_shifts = this->bit_shifts;
				output[segment_index].data.assign(output[segment_index].size, block<oT>());
			}

			block<oT>& current = output[segment_index].data[block_index];
			for (size_t k = 0; k < this->c_families_count; ++k) {
				size_t offset = 1;
				size_t index = 0;
				size_t size = 1;
				size_t stride = 1;
				current.content[k + offset] = 
					this->handle_item_pack(this->buffers[k + 1][i][j]);
				
				index = 0;
				offset += 3 * size;
				stride = 2;
				size = stride * stride;
				for (size_t ii = 0; ii < 2; ++ii) {
					for (size_t jj = 0; jj < 2; ++jj) {
						T& coeff = this->buffers[k + 4][i * stride + ii][j * stride + jj];
						current.content[offset + k * size + index] = 
							this->handle_item_pack(coeff);
						++index;
					}
				}

				index = 0;
				offset += 3 * size;
				stride = 4;
				size = stride * stride;
				for (size_t ii = 0; ii < 2; ++ii) {
					for (size_t jj = 0; jj < 2; ++jj) {
						for (size_t iii = 0; iii < 2; ++iii) {
							for (size_t jjj = 0; jjj < 2; ++jjj) {
								T& coeff = 
									this->buffers[k + 7]
									[i * stride + ii * 2 + iii][j * stride + jj * 2 + jjj];
								current.content[offset + k * size + index] =
									this->handle_item_pack(coeff);
								++index;
							}
						}
					}
				}
			}
			++block_index;
		}
	}
	return output;
}

template<typename T, size_t alignment>
inline void kdiff(T* input, T* output, size_t length, size_t bdepth, bool normalize = false) {
	// mask covers half of static range
	T mask = ~((-1 << bdepth) >> 1);
	// norm is either 0 either half of static range
	T norm = (1 << (bdepth & (-((ptrdiff_t)(normalize))))) >> 1;
	for (size_t ii = 0; ii < length; ii += alignment) {
		for (size_t jj = 0; jj < alignment; ++jj) {
			// see 4.3.2.4
			// see comments for transformation below after the function body
			output[ii + jj] = input[ii + jj + 1] - input[ii + jj];
			// the trick below with norm is to unify the function for 
			// both qDC and bdepthAcBlocks
			T normalized = input[ii + jj] - norm;
			T theta = (normalized ^ (~(normalized >> ((sizeof(T) << 3) - 1)))) & mask;

			// effectively codes sign bit in the least significant bit
			// allows applying xor by extended sign value
			// sign = n & 0x01
			// base = n >> 1
			// value = base ^ signext(sign)
			T sign = output[ii + jj] >> ((sizeof(T) << 3) - 1);
			output[ii + jj] = output[ii + jj] ^ sign;
			// this implements strict less than
			T rel = (theta - output[ii + jj]) >> ((sizeof(T) << 3) - 1);
			output[ii + jj] += theta & rel;
			output[ii + jj] += output[ii + jj] & (~rel);
			output[ii + jj] -= sign;
		}
	}
}

// As per paragraph 4.3.2.4, p. 4-20
// delta[m]' = c[m] - c[m-1]
// 
// rules:
// delta[m] = 2*delta[m]'				: 0 <= delta[m]' <= theta			(1)
// delta[m] = 2*abs(delta[m]') - 1		: -theta <= delta[m]' < 0			(2)
// delta[m] = theta + abs(delta[m]')	otherwise							(3)
// 
// 
// Obviously, 
// abs(delta[m]') == delta[m]' : delta[m]' >= 0
// (-theta <= delta[m]' <= 0)  <=>  (0 <= abs(delta[m]') <= theta)		: delta[m]' < 0
// 
// 
// Let's define signext(t) as the following:
// {
//		t >> (bitsize(t) - 1)
// }
// (that effectively extends sign bit throughout all the mantiss; note that 
// signext(t) == -1			: t < 0
// signext(t) == 0			: t >= 0)
// 
// 
// Let's define signxor(t) as the following:
// {
//		(signext(t) ^ t)  
// <=>	(t >> (bitsize(t) - 1)) ^ t
// 
//		in C++ syntax:
// <=>	(t >> ((sizeof(t) << 3) - 1)) ^ t
// }
//
// Note that
// abs(t) == signxor(t) - signext(t)
// abs(t) == signxor(t)		: t >= 0
// abs(t) == signxor(t) + 1	: t < 0
// 
// 
// Note that in the original rules defined in 4.3.2.4, both rules (1) and (2) produce the same result value
// when (delta[m]' == theta). This can be used to decrease the high range boundary by 1 for equation (1).
// However, results of (2) and (3) are different when (abs(delta[m]') == theta), equation (3) is not 
// applicable.
// 
// 
// Rewruting equtaion (2) using signxor instead of abs:
// delta[m] = 2*abs(delta[m]') - 1		: -theta <= delta[m]' <= 0
//	<=>
// delta[m] = 2*signxor(delta[m]') + 1	: {(0 < (signxor(delta[m]') - signext(delta[m]')) <= theta); (delta[m]' < 0)} 
//  <=>
// delta[m] = 2*signxor(delta[m]') - signext(delta[m]') 	: {(0 < (signxor(delta[m]') + 1) <= theta); (delta[m]' < 0)} 
// 
// Taking into accaount that delta[m]' and theta are integers, we can transform righ-hand part of unequation
// to strict less than (<), subtracting 1 from the middle part.
// delta[m] = 2*signxor(delta[m]') - signext(delta[m]') 	: {(0 < signxor(delta[m]') < theta); (delta[m]' < 0)} 
// 
// 
// For equation (1) transform is straight-forward, just to use another notation:
// delta[m] = 2*delta[m]'				: 0 <= delta[m]' <= theta
//  <=>
// delta[m] = 2*abs(delta[m]')				: {(0 <= abs(delta[m]') <= theta), (delta[m]' >= 0)}
//  <=>
// delta[m] = 2*(signxor(delta[m]') - signext(delta[m]'))	
//			: {(0 <= (signxor(delta[m]') - signext(delta[m]')) <= theta), (delta[m]' >= 0)}
//  <=> (signext(delta[m]') == 0 : delta[m]' >= 0)
// delta[m] = 2*signxor(delta[m]') - signext(delta[m]')	: {(0 <= signxor(delta[m]') <= theta), (delta[m]' >= 0)}
//  <=> (reducing unequality range as explained above)
// delta[m] = 2*signxor(delta[m]') - signext(delta[m]')	: {(0 <= signxor(delta[m]') < theta), (delta[m]' >= 0)}
// 
// 
// Rewriting rules defined in 4.3.2.4 using signxor instead of abs finally results in:
// delta[m] = 2*signxor(delta[m]') - signext(delta[m]')			0 <= signxor(delta[m]') < theta
// delta[m] = theta + signxor(delta[m]') - signext(delta[m]'	otherwise
// 

template<typename T, size_t alignment>
std::vector<segment<typename SegmentAssembler<T, alignment>::oT>> SegmentAssembler<T, alignment>::apply() {
	std::vector<segment<oT>> output = this->pack();
	for (size_t i = 1; i < this->buffers.size(); ++i) {
		// free buffers, they are not necessary once we packed all values
		this->buffers[i] = bitmap<T, alignment>();
	}

	{
		// TODO: see segment validation in bpe (kEncode requirements)
		constexpr size_t items_per_gaggle = 16;
		constexpr size_t k_offset_requirement = items_per_gaggle * 2;

		size_t dc_index = 0;
		for (size_t i = 0; i < output.size(); ++i) {
			output[i].plainDc = aligned_vector<oT>(output[i].size);
			output[i].quantizedDc = aligned_vector<oT>(output[i].size, k_offset_requirement);
			output[i].quantizedBdepthAc = aligned_vector<size_t>(output[i].size, k_offset_requirement);
			this->buffers[0].linear(output[i].plainDc.data(), output[i].size, dc_index);
			output[i].bdepthDc = bdepthv<T, alignment>(output[i].plainDc.data(), output[i].size);

			std::make_unsigned_t<oT> magnitude_mask_acc = 0;
			for (ptrdiff_t j = 0; j < output[i].size; ++j) {
				std::make_unsigned_t<oT> magnitude_mask = accorv<oT, alignment>(output[i].data[j].content, this->c_block_item_count);
				output[i].quantizedBdepthAc[j] = std::bit_width(magnitude_mask); // +1; // see 4.1, p. 4-3, eq (13)
				magnitude_mask_acc |= magnitude_mask;
			}
			output[i].bdepthAc = std::bit_width(magnitude_mask_acc);
			// dc coefficients are not assigned to block[0] in pack function, hence bdepth below 
			// is effective for ac only
			// output[i].bdepthAc = bdepthv<T, alignment>(output[i].data.data()->content, output[i].size * this->c_block_item_count);
			dc_index += output[i].size;

		}
		this->buffers[0] = bitmap<T, alignment>();
	}

	aligned_vector<oT> plainDcBuffer(output[0].size);
	aligned_vector<size_t> bdepthAcBuffer(output[0].size);

	constexpr size_t palignment = alignment * sizeof(oT);
	for (size_t i = 0; i < output.size(); ++i) {
		plainDcBuffer.resize(output[i].size);
		bdepthAcBuffer.resize(output[i].size);
		oT* segmentDc = output[i].plainDc.data();
		output[i].q = quant_dc(output[i].bdepthDc, output[i].bdepthAc, output[i].bit_shifts[0]);

		oT mask = ~(-1 << output[i].q);

		for (ptrdiff_t ii = 0; ii < output[i].size; ii += alignment) {
			for (ptrdiff_t jj = 0; jj < alignment; ++jj) {
				plainDcBuffer[ii + jj] = segmentDc[ii + jj] & mask; // PERF NOTE: by pointer assigned to subscripted
				segmentDc[ii + jj] >>= output[i].q;
			}
		}
		output[i].referenceSample = segmentDc[0];

		// PERF NOTE: call to new() contains side effects and therefore considered 
		// serializing op. Moved to the first loop due to performance reasons.
		// output[i].quantizedDc = aligned_vector<T>(output[i].size); // TODO: move to first loop?
		// T* diffData = output[i].quantizedDc.data();

		// Below is the same as kdiff function but works for quantized DC only. 
		// I decided to keep it in a comments for future reference.
		// 
		// mask = ~(-1 << (output[i].bdepthDc - output[i].q));
		// for (size_t ii = 0; ii < output[i].size; ii += alignment) {
		// 	for (size_t jj = 0; jj < alignment; ++jj) {
		// 		// see 4.3.2.4
		// 		// see comments for transformation below after the function body
		// 		diffData[ii + jj] = segmentDc[ii + jj + 1] - segmentDc[ii + jj];
		// 		T theta = (segmentDc[ii + jj] ^ (~(segmentDc[ii + jj] >> ((sizeof(T) << 3) - 1)))) & mask;
		// 
		// 		// effectively codes sign bit in the least significant bit
		// 		// allows applying xor by extended sign value
		// 		// sign = n & 0x01
		// 		// base = n >> 1
		// 		// value = base ^ signext(sign)
		// 		T sign = diffData[ii + jj] >> ((sizeof(T) << 3) - 1);
		// 		diffData[ii + jj] = diffData[ii + jj] ^ sign;
		// 		// this implements strict less than
		// 		T rel = (theta - diffData[ii + jj]) >> ((sizeof(T) << 3) - 1);
		// 		diffData[ii + jj] += theta & rel;
		// 		diffData[ii + jj] += diffData[ii + jj] & (~rel);
		// 		diffData[ii + jj] -= sign;
		// 	}
		// }
		// 

		// (output[i].bdepthDc - output[i].q) can be negative because q is 
		// computed as max(q`, BitShiftLL3) and therefore van be greater than 
		// bdepthDc when bdepthDc is 1 due to LL3 subband being all zeroes.
		// See bpe kEncode for details.
		// size_t bdepthQDc = (output[i].bdepthDc - output[i].q) + 1;
		ptrdiff_t bdepthQDc = (((ptrdiff_t)(output[i].bdepthDc)) - output[i].q) + 1; // TODO: 
		// yet not clear why I put +1 here and why below is checked against 
		// 'gt' 1. Check the same in decode segment chain.

		// see 4.3.2.2: skip kdiff if N == 1
		if (bdepthQDc > 1) {
			oT* diffData = output[i].quantizedDc.data();
			kdiff<oT, alignment>(segmentDc, diffData, output[i].size, bdepthQDc, false);
			swap(output[i].plainDc, plainDcBuffer); // do not expose temporal values to the caller
		} else {
			swap(output[i].plainDc, output[i].quantizedDc);
		}

		// see 4.4, eq.21: N = round_up(log(1 + bdepthAc))
		// std::bit_depth = 1 + round_down(log(x))
		// 
		// 1 + round_down(log(x)) == round_up(log(1 + x)) | for all natural x
		size_t bdepthAcBdepth = std::bit_width(output[i].bdepthAc);
		if (bdepthAcBdepth > 1) {
			ptrdiff_t* bdepthsAc = (ptrdiff_t*)output[i].quantizedBdepthAc.data();
			output[i].referenceBdepthAc = bdepthsAc[0];
			ptrdiff_t* diffBdipthAc = (ptrdiff_t*)bdepthAcBuffer.data();
			kdiff<ptrdiff_t, alignment>(bdepthsAc, diffBdipthAc, output[i].size, bdepthAcBdepth, true);
			swap(output[i].quantizedBdepthAc, bdepthAcBuffer); // do not expose temporal values to the caller
		}

		// TODO: bdepth AC blocks values are encoded per standard, but not required to be used
		// during BPE encoding/decoding, and are not actually used in this implementation.
		// Maybe possible to use for performance improvement during BPE decoding or AC 
		// approximation when coded data terminates early.
		// 

		// TODO: implement constexpr if to enable the block below in 
		// debug builds only
		// 
		// debug block, reverse op
		{
			oT* diffData = output[i].quantizedDc.data();
			diffData[-1] = output[i].referenceSample;
			mask = ~(-1 << (output[i].bdepthDc - output[i].q));
			for (ptrdiff_t j = 0; j < output[i].size - 1; ++j) {
				oT signmask = segmentDc[j] >> ((sizeof(oT) << 3) - 1);
				oT theta = (segmentDc[j] ^ (~signmask)) & mask;
				oT predicate = diffData[j] - theta;
				oT val = diffData[j];
				oT diff = ((-(val & 0x01)) ^ (val >> 1));
				if (predicate > theta) {
					diff = -(predicate ^ signmask) + signmask;
				}

				oT restoredDcCoeff = segmentDc[j] + diff;
				if (restoredDcCoeff != segmentDc[j + 1]) {
					throw "NEQ!";
				}
			}
		}
		{
			ptrdiff_t* diffBdipthAc = (ptrdiff_t*)output[i].quantizedBdepthAc.data();
			ptrdiff_t* bdepthsAc = (ptrdiff_t*)bdepthAcBuffer.data();
			diffBdipthAc[-1] = output[i].referenceBdepthAc;
			mask = ~((-1 << bdepthAcBdepth) >> 1);
			ptrdiff_t norm = (1 << bdepthAcBdepth) >> 1;
			for (ptrdiff_t j = 0; j < output[i].size - 1; ++j) {
				ptrdiff_t normalized = bdepthsAc[j] - norm;
				ptrdiff_t signmask = normalized >> ((sizeof(ptrdiff_t) << 3) - 1);
				ptrdiff_t theta = (normalized ^ (~signmask)) & mask;
				ptrdiff_t predicate = diffBdipthAc[j] - theta;
				ptrdiff_t val = diffBdipthAc[j];
				ptrdiff_t diff = ((-(val & 0x01)) ^ (val >> 1));
				if (predicate > theta) {
					diff = -(predicate ^ signmask) + signmask;
				}

				T restoredBdepthAcCoeff = bdepthsAc[j] + diff;
				if (restoredBdepthAcCoeff != bdepthsAc[j + 1]) {
					throw "NEQ!";
				}
			}
		}
	}

	return output;
}

template <typename T, size_t alignment>
SegmentAssembler<T, alignment>::oT SegmentAssembler<T, alignment>::handle_item_pack(T coeff) const {
	return (oT)coeff;
}

template <typename T, size_t alignment>
void SegmentAssembler<T, alignment>::set_shifts(const shifts_t& shifts) {
	this->bit_shifts = shifts;
}

template <typename T, size_t alignment>
shifts_t SegmentAssembler<T, alignment>::get_shifts() const {
	return this->bit_shifts;
}

// SegmentDisassembler class template implementation

template <typename T, size_t alignment>
SegmentDisassembler<T, alignment>::SegmentDisassembler(std::vector<segment<iT>> input): segments(input) {}

template <typename T, size_t alignment>
inline void rkdiff(T* diffData, size_t length, size_t bdepth, bool normalize = false) {
	// mask covers half of static range
	T mask = ~((-1 << bdepth) >> 1);
	// norm is either 0 either half of static range
	T norm = (1 << (bdepth & (-((ptrdiff_t)(normalize))))) >> 1;
	for (ptrdiff_t j = 0; j < length; ++j) {
		T normalized = diffData[j - 1] - norm;
		T signmask = normalized >> ((sizeof(T) << 3) - 1);
		T theta = (normalized ^ (~signmask)) & mask;
		T predicate = diffData[j] - theta;
		T val = diffData[j];
		T diff = ((-(val & 0x01)) ^ (val >> 1));
		if (predicate > theta) {
			diff = -(predicate ^ signmask) + signmask;
		}

		diffData[j] = diffData[j - 1] + diff;
	}
}

template <typename T, size_t alignment>
SegmentDisassembler<T, alignment>::output_t SegmentDisassembler<T, alignment>::apply(size_t image_width) {
	for (ptrdiff_t i = 0; i < this->segments.size(); ++i) {
		// reference sample:
		this->segments[i].data[0].content[0] = 
			(this->segments[i].referenceSample << this->segments[i].q) | this->segments[i].plainDc[0];
		T* diffData = this->segments[i].quantizedDc.data();
		diffData[-1] = this->segments[i].referenceSample;

		// Below is the same as rkdiff function but works for quantized DC only. 
		// I decided to keep it in a comments for future reference.
		// Note that inlined implementation moves the coefficient to the corresponding
		// block at the end of the loop.
		// 
		// T mask = ~(-1 << (this->segments[i].bdepthDc - this->segments[i].q));
		// for (ptrdiff_t j = 0; j < this->segments[i].size - 1; ++j) {
		// 	T signmask = diffData[j - 1] >> ((sizeof(T) << 3) - 1);
		// 	T theta = (diffData[j - 1] ^ (~signmask)) & mask;
		// 	T predicate = diffData[j] - theta;
		// 	T val = diffData[j];
		// 	T diff = ((-(val & 0x01)) ^ (val >> 1));
		// 	if (predicate > theta) {
		// 		diff = -(predicate ^ signmask) + signmask;
		// 	}
		// 
		// 	diffData[j] = diffData[j - 1] + diff;
		// 	this->segments[i].data[j + 1].content[0] |= diffData[j] << this->segments[i].q;
		// }
		size_t bdepthQDc = (this->segments[i].bdepthDc - this->segments[i].q) + 1;
		if (bdepthQDc > 1) {
			rkdiff<T, alignment>(diffData, (this->segments[i].size - 1), bdepthQDc, false);
			for (ptrdiff_t j = 0; j < this->segments[i].size - 1; ++j) {
				this->segments[i].data[j + 1].content[0] = 
					(diffData[j] << this->segments[i].q) | this->segments[i].plainDc[j + 1]; // PERF NOTE: access by ptr and subscript operator
			}
		}

		size_t bdepthAcBdepth = std::bit_width(this->segments[i].bdepthAc);
		if (bdepthAcBdepth > 1) {
			ptrdiff_t* diffBdepthAc = (ptrdiff_t*)this->segments[i].quantizedBdepthAc.data();
			diffBdepthAc[-1] = this->segments[i].referenceBdepthAc;
			rkdiff<ptrdiff_t, alignment>(diffBdepthAc, (this->segments[i].size - 1), bdepthAcBdepth, true);
		}
	}

	return this->unpack(image_width);
}

template <typename T, size_t alignment>
SegmentDisassembler<T, alignment>::output_t SegmentDisassembler<T, alignment>::unpack(size_t image_width) {
	constexpr size_t min_img_width = 17;
	constexpr size_t img_granularity = 16;
	image_width = std::max(image_width, min_img_width);
	image_width = (image_width + img_granularity - 1) & (~(img_granularity - 1));

	// TODO: needs proper image dimension checks to comply with requirements
	// (but checks may be performed on a previous stage)
	img_meta ll3_meta = [](size_t _image_width) noexcept -> img_meta {
		// limits in target image dimensions
		constexpr size_t height_limit = 0x01 << 11;
		constexpr size_t length_limit = 0x01 << 28;
		img_meta target{};
		target.width = _image_width;
		if (_image_width < height_limit) {
			target.height = _image_width;
		}
		else {
			size_t length = _image_width * _image_width;
			if (length < length_limit) {
				target.height = height_limit;
			}
			else {
				target.height = ((length_limit / _image_width) + 
					img_granularity - 1) & (~(img_granularity - 1));;
			}
		}

		// cast to LL3 dims
		target.width >>= 3;
		target.height >>= 3;
		target.length = target.width * target.height;
		return target;
	}(image_width);

	auto init_buffers_f = [](size_t width, size_t height) {
		subbands_t<T> buffers;
		constexpr size_t c_level_count = 3;
		constexpr size_t buffer_iter_step = 3;
		buffers[0].resize(width, height);
		for (size_t i = 0; i < c_level_count; ++i) {
			for (ptrdiff_t j = 0; j < buffer_iter_step; ++j) {
				buffers[i * buffer_iter_step + j + 1].resize(width, height);
			}
			width <<= 1;
			height <<= 1;
		}
		return buffers;
	};

	output_t buffers_collection;

	// TODO: may need an offset if segments are processed concurrently.
	// inherently requires bitmap image offset since segments are not alligned
	// per image boundaries. But offset is applicable to the first buffer row 
	// only and is (img.width - 1) at most.
	constexpr ptrdiff_t overlap_rows = 16;
	ptrdiff_t base_row_i = 0;
	ptrdiff_t base_col_i = ll3_meta.height; // that triggers allocation of buffers
	for (ptrdiff_t i = 0; i < this->segments.size(); ++i) {
		ptrdiff_t j = 0;
		while (j < this->segments[i].size) {
			// create new buffer if needed
			// Copy coeffs from previous buffer set to the new one
			// so that data overlaps, permitting concurrent processing 
			// of output buffers by several DWT simultaneously. This 
			// allows to merge DWT output.
			if (base_col_i == ll3_meta.height) {
				buffers_collection.emplace_back(init_buffers_f(ll3_meta.width, ll3_meta.height));
				if (buffers_collection.size() >= 2) {
					auto src = buffers_collection[buffers_collection.size() - 2];
					auto dst = buffers_collection[buffers_collection.size() - 1];
					for (ptrdiff_t level = 0; level < 3; ++level) {
						ptrdiff_t level_overlap = overlap_rows << level;
						for (ptrdiff_t level_offset = (level > 0); level_offset < 4; ++level_offset) {
							ptrdiff_t buffer_index = 3 * level + level_offset;
							overlapBitmaps(src[buffer_index], dst[buffer_index], level_overlap);
						}
					}
				}

				base_row_i = 0;
				base_col_i = 0;
			}

			size_t buffer_linear_index = base_row_i * ll3_meta.width + base_col_i;
			// check if multiple segments fit one buffer (j is 0 on segment start 
			// and buffers may not be empty)
			ptrdiff_t segment_bound = j + 
				std::min(ll3_meta.length - buffer_linear_index, this->segments[i].size - j);
			for (; j < segment_bound; ++j) {
				ptrdiff_t l = 0;
				ptrdiff_t disp[3] = { 0 };

				// compact implimentation, lacks memory locality, but should benefit from well predictable 
				// and uniform branching
				// section 4.1, p.27
				// this magic needs some clarification:
				// Coefficients populate the bitmap group by group, starting from DC at index 0
				// groups are: (DC), (Pi), (Ci), (Gi); handled left to right. 
				// Level variable represents the current group.
				// Binary representation of item index inside of a block 
				// correlates with item position indicies in the bitmap
				// when treated by bit pairs. Item index inside of a block 
				// is treated according to the current group (therefore 
				// number of displacemet coefficients computed from the 
				// index increases from 0 up to 3). Relu function is 
				// necessary to make implementation work for both DC group
				// consisting of a single item at index 0 and Parents 
				// group with items at indicies [1...3].
				// Implementation can be generlized for 3+ level DWT 
				for (ptrdiff_t bound_i = 0; bound_i <= 6; bound_i += 2) {
					ptrdiff_t bound = 1ll << bound_i;
					ptrdiff_t level = bound_i >> 1;
					for (; l < bound; ++l) {
						ptrdiff_t mask = (0b11 << bound_i) >> 2;
						for (ptrdiff_t k = 0; mask > 0; ++k, mask >>= 2) {
							disp[k] = (l & mask) >> ((level - k - 1) << 1);
						}
						buffers_collection.back()[3 * relu(level - 1) + disp[0]]
							[(base_row_i << relu(level - 1)) +
								((disp[1] >> 1) * relu(level - 1)) +
								((disp[2] >> 1) * relu(level - 2))]
							[(base_col_i << relu(level - 1)) +
								((disp[1] & 0x01) * relu(level - 1)) +
								((disp[2] & 0x01) * relu(level - 2))]
							= this->handle_item_unpack(this->segments[i].data[j].content[l]);
					}
				}

				++base_col_i;
				base_row_i += (base_col_i >= ll3_meta.width);
				base_col_i &= (base_col_i >= ll3_meta.width) - 1;
			}
		}
	}
	base_row_i += (base_col_i > 0);
	for (ptrdiff_t level = 0; level < 3; ++level) {
		for (ptrdiff_t level_offset = (level > 0); level_offset < 4; ++level_offset) {
			ptrdiff_t buffer_index = 3 * level + level_offset;
			buffers_collection.back()[buffer_index].shrink(base_row_i << level);
		}
	}

	return buffers_collection;
}

template <typename T, size_t alignment>
T SegmentDisassembler<T, alignment>::handle_item_unpack(iT coeff) const {
	return (T)coeff;
}
