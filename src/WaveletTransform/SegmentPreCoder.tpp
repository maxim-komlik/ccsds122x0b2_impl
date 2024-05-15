#pragma once

// TODO: refactor include paths
#include "core_types.h"
#include "utils.h"
#include "bitmap.tpp"

#include <array>
#include <vector>
#include <bit>

template <typename T, size_t alignment = 16>
class SegmentPreCoder {
	std::array<typename bitmap<T>, 10> buffers;
	size_t segment_size;

	static constexpr size_t c_families_count = 3;
	static constexpr size_t c_block_item_count = 64;
public:
	SegmentPreCoder(std::array<typename bitmap<T>, 10> input);
	SegmentPreCoder(std::vector<typename bitmap<T>>& input);
	std::vector<segment<T>> apply();
private:
	std::vector<segment<T>> pack();
	// let here compiler do necessary type conversion to avoid unsigned underflow
	inline size_t segmentQ(ptrdiff_t bdepthDc, ptrdiff_t bdepthAc) noexcept;
};

template <typename T, size_t alignment = 16>
class SegmentPostDecoder {
	std::vector<typename segment<T>> segments;

	static constexpr size_t c_families_count = 3;
public:
	typedef std::vector<typename std::array<typename bitmap<T>, 10>> output_t;

	SegmentPostDecoder(std::vector<segment<T>> input);
	output_t apply(size_t image_width);
private:
	output_t unpack(size_t image_width);
};

// SegmentPreCoder class template implementation

template <typename T, size_t alignment>
SegmentPreCoder<T, alignment>::SegmentPreCoder(std::array<typename bitmap<T>, 10> input) : buffers(input), segment_size(0) {}

template <typename T, size_t alignment>
SegmentPreCoder<T, alignment>::SegmentPreCoder(std::vector<typename bitmap<T>>& input): segment_size(0) {
	size_t basei = input.size() - this->buffers.size();
	// TODO: validate

	for (size_t i = 0; i < this->buffers.size(); ++i) {
		this->buffers[i] = input[basei + i];
	}
}

template <typename T, size_t alignment>
std::vector<segment<T>> SegmentPreCoder<T, alignment>::pack() {
	// Does not pack DC coefficients into blocks, AC only

	img_meta DC_loc = this->buffers[0].getImgMeta();
	size_t block_count = (DC_loc.width * DC_loc.height);
	if (this->segment_size == 0) {
		this->segment_size = block_count;
	}
	size_t segment_count = (block_count + (this->segment_size - 1)) / this->segment_size;
	std::vector<segment<T>> output(segment_count); // implicitly calls default ctor for vector, size = 0
	--segment_count;
	// std::vector<block<T>> output(DC_loc.width * DC_loc.height);

	size_t segment_index = 0, block_index = 0;
	output[segment_index].size = segment_index < segment_count ? this->segment_size : block_count;
	output[segment_index].data.assign(output[segment_index].size, block<T>());
	for (size_t i = 0; i < DC_loc.height; ++i) {
		for (size_t j = 0; j < DC_loc.width; ++j) {
			if (block_index >= this->segment_size) {
				block_index = 0;
				++segment_index;
				output[segment_index].size = segment_index < segment_count ?
					this->segment_size : block_count % this->segment_size;
				output[segment_index].data.assign(output[segment_index].size, block<T>());
			}

			block<T>& current = output[segment_index].data[block_index];
			for (size_t k = 0; k < this->c_families_count; ++k) {
				size_t offset = 1;
				size_t index = 0;
				size_t size = 1;
				size_t stride = 1;
				current.content[k + offset] = this->buffers[k + 1][i][j];

				index = 0;
				offset += 3 * size;
				stride = 2;
				size = stride * stride;
				for (size_t ii = 0; ii < 2; ++ii) {
					for (size_t jj = 0; jj < 2; ++jj) {
						current.content[offset + k * size + index] =
							this->buffers[k + 4][i * stride + ii][j * stride + jj];
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
								current.content[offset + k * size + index] =
									this->buffers[k + 7]
									[i * stride + ii * 2 + iii][j * stride + jj * 2 + jjj];
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
std::vector<segment<T>> SegmentPreCoder<T, alignment>::apply() {
	// TODO:
	// AC values: sign bit + magnitude (see p. 4-3)
	// TODO: perform scaling of the input depending on its type (ceiling or shifting)
	std::vector<segment<T>> output = this->pack();
	for (size_t i = 1; i < this->buffers.size(); ++i) {
		// free buffers, they are not necessary once we packed all values
		this->buffers[i] = bitmap<T, alignment>();
	}

	size_t dc_index = 0;
	for (size_t i = 0; i < output.size(); ++i) {
		output[i].plainDc = aligned_vector<T>(output[i].size);
		output[i].quantizedDc = aligned_vector<T>(output[i].size);
		output[i].quantizedBdepthAc = aligned_vector<size_t>(output[i].size);
		T* segmentDc = output[i].plainDc.data();
		this->buffers[0].linear(segmentDc, output[i].size, dc_index);
		output[i].bdepthDc = bdepthv<T, alignment>(segmentDc, output[i].size);

		std::make_unsigned_t<T> magnitude_mask_acc = 0;
		for (ptrdiff_t j = 0; j < output[i].size; ++j) {
			std::make_unsigned_t<T> magnitude_mask = accorv<T, alignment>(output[i].data[j].content, this->c_block_item_count);
			output[i].quantizedBdepthAc[j] = std::bit_width(magnitude_mask) + 1; // see 4.1, p. 4-3, eq (13)
			magnitude_mask_acc |= magnitude_mask;
		}
		output[i].bdepthAc = std::bit_width(magnitude_mask_acc);
		// dc coefficients are not assigned to block[0] in pack function, hence bdepth below 
		// is effective for ac only
		// output[i].bdepthAc = bdepthv<T, alignment>(output[i].data.data()->content, output[i].size * this->c_block_item_count);
		dc_index += output[i].size;
	}

	aligned_vector<T> plainDcBuffer(output[0].size);
	aligned_vector<size_t> bdepthAcBuffer(output[0].size);

	constexpr size_t palignment = alignment * sizeof(T);
	for (size_t i = 0; i < output.size(); ++i) {
		plainDcBuffer.resize(output[i].size);
		bdepthAcBuffer.resize(output[i].size);
		T* segmentDc = output[i].plainDc.data();
		output[i].q = this->segmentQ(output[i].bdepthDc, output[i].bdepthAc);

		T mask = ~(-1 << output[i].q);
		// for (size_t j = 0; j < output[i].size; ++j) {
		// 	output[i].data[j].content[0] = segmentDc[j] & mask;
		// }

		for (size_t ii = 0; ii < output[i].size; ii += alignment) {
			for (size_t jj = 0; jj < alignment; ++jj) {
				plainDcBuffer[ii + jj] = segmentDc[ii + jj] & mask; // PERF NOTE: by pointer assigned to subscripted
				segmentDc[ii + jj] >>= output[i].q;
			}
		}
		output[i].referenceSample = segmentDc[0];
		// TODO: see 4.3.2.2: skip kdiff if N == 1
		// may need to move output[i].plainDc to output[i].quantizedDc and reinitialize plainDc

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

		size_t bdepthQDc = (output[i].bdepthDc - output[i].q) + 1;
		// TODO: see 4.3.2.2: skip kdiff if N == 1
		// may need to move output[i].plainDc to output[i].quantizedDc and reinitialize plainDc
		if (bdepthQDc > 1) {
			T* diffData = output[i].quantizedDc.data();
			kdiff<T, alignment>(segmentDc, diffData, output[i].size, bdepthQDc, false);
			swap(output[i].plainDc, plainDcBuffer); // do not expose temporal values to the caller
		} else {
			swap(output[i].plainDc, output[i].quantizedDc);
		}

		// see 4.4 (c): N = round_up(log(1 + bdepthAc))
		// TODO: check formulas, see bit_width and eq.21 from 4.4
		size_t bdepthAcBdepth = std::bit_width(output[i].bdepthAc);
		if (bdepthAcBdepth > 1) {
			ptrdiff_t* bdepthsAc = (ptrdiff_t*)output[i].quantizedBdepthAc.data();
			output[i].referenceBdepthAc = bdepthsAc[0];
			// output[i].quantizedBdepthAc = aligned_vector<size_t>(output[i].size);
			// ptrdiff_t* diffBdipthAc = (ptrdiff_t*)output[i].quantizedBdepthAc.data();
			ptrdiff_t* diffBdipthAc = (ptrdiff_t*)bdepthAcBuffer.data();
			//constexpr size_t bdepthAcBdepth = std::bit_width((sizeof(T) << 3) - 1); // bit_width(max_value)
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
			T* diffData = output[i].quantizedDc.data();
			diffData[-1] = output[i].referenceSample;
			mask = ~(-1 << (output[i].bdepthDc - output[i].q));
			for (ptrdiff_t j = 0; j < output[i].size - 1; ++j) {
				T signmask = segmentDc[j] >> ((sizeof(T) << 3) - 1);
				T theta = (segmentDc[j] ^ (~signmask)) & mask;
				T predicate = diffData[j] - theta;
				T val = diffData[j];
				T diff = ((-(val & 0x01)) ^ (val >> 1));
				if (predicate > theta) {
					diff = -(predicate ^ signmask) + signmask;
				}

				T restoredDcCoeff = segmentDc[j] + diff;
				if (restoredDcCoeff != segmentDc[j + 1]) {
					throw "NEQ!";
				}
			}
		}
		{
			ptrdiff_t* diffBdipthAc = (ptrdiff_t*)output[i].quantizedBdepthAc.data();
			ptrdiff_t* bdepthsAc = (ptrdiff_t*)bdepthAcBuffer.data();
			diffBdipthAc[-1] = output[i].referenceBdepthAc;
			mask = ~(-1 << bdepthAcBdepth);
			T norm = (1 << bdepthAcBdepth) >> 1;
			for (ptrdiff_t j = 0; j < output[i].size - 1; ++j) {
				T normalized = bdepthsAc[j] - norm;
				T signmask = normalized >> ((sizeof(T) << 3) - 1);
				T theta = (normalized ^ (~signmask)) & mask;
				T predicate = diffBdipthAc[j] - theta;
				T val = diffBdipthAc[j];
				T diff = ((-(val & 0x01)) ^ (val >> 1));
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
size_t SegmentPreCoder<T, alignment>::segmentQ(ptrdiff_t bdepthDc, ptrdiff_t bdepthAc) noexcept {
	bdepthAc = (bdepthAc >> 1) + 1;
	// avoids jumps and hints to use conditional moves here
	if (bdepthDc - bdepthAc <= 1) {
		bdepthAc = bdepthDc - 3;
	}
	if (bdepthDc - bdepthAc > 10) {
		bdepthAc = bdepthDc - 10;
	}
	// result is guaranteed to be positive, implicit conversion to size_t here
	// See 4.3.1.3
	return std::max<decltype(bdepthAc)>(bdepthAc, 3); // TODO: Ooops, here should be custom weight instead of hardcoded
}

// SegmentPostDecoder class template implementation

template <typename T, size_t alignment>
SegmentPostDecoder<T, alignment>::SegmentPostDecoder(std::vector<segment<T>> input): segments(input) {}

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
SegmentPostDecoder<T, alignment>::output_t SegmentPostDecoder<T, alignment>::apply(size_t image_width) {
	for (ptrdiff_t i = 0; i < this->segments.size(); ++i) {
		// reference sample:
		// this->segments[i].data[0].content[0] |= this->segments[i].referenceSample << this->segments[i].q;
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
				// this->segments[i].data[j + 1].content[0] |= diffData[j] << this->segments[i].q;
				this->segments[i].data[j + 1].content[0] = 
					(diffData[j] << this->segments[i].q) | this->segments[i].plainDc[j + 1]; // PERF NOTE: access by ptr and subscript operator
			}
		}

		size_t bdepthAcBdepth = std::bit_width(this->segments[i].bdepthAc);
		if (bdepthAcBdepth > 1) {
			ptrdiff_t* diffBdepthAc = (ptrdiff_t*)this->segments[i].quantizedBdepthAc.data();
			diffBdepthAc[-1] = this->segments[i].referenceBdepthAc;
			// constexpr size_t bdepthAcBdepth = std::bit_width((sizeof(T) << 3) - 1); // bit_width(max_value)
			rkdiff<ptrdiff_t, alignment>(diffBdepthAc, (this->segments[i].size - 1), bdepthAcBdepth, true);
		}
	}

	// do unpack here
	return this->unpack(image_width);
}

template <typename T, size_t alignment>
SegmentPostDecoder<T, alignment>::output_t SegmentPostDecoder<T, alignment>::unpack(size_t image_width) {
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

	auto init_buffers_f =
		[](size_t width, size_t height) {
			std::array<bitmap<T>, 10> buffers;
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
							= this->segments[i].data[j].content[l];
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
