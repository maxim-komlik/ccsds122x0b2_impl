#pragma once

#include <array>
#include <vector>
#include <deque>
#include <bit>
#include <numeric>
#include <type_traits>

#include "core_types.h"
#include "utils.h"

#include "aligned_vector.tpp"

#include "obitwrapper.tpp"
#include "ibitwrapper.tpp"
#include "bitplane.tpp"
#include "symbolstream.h"
#include "symbol_translate.h"
#include "entropy_translate.h"

#include "bpe_constants.h"

// The class below is BitPlaneEncoder and BitPlaneDecoder combined, 
// implementation duplicates the code provided for single-direction 
// transformation classes. BitPlaneEncoder and BitPlaneDecoder use several 
// common members the same way, and those occupy significant amount of memory.
//

template <typename T, size_t alignment = 16>
class BitPlaneXcoder: private bpe_constants_helper {
	// inherently not copyable nor movable
	SymbolForwardTranslator symbol_fwd_translator;
	SymbolBackwardTranslator symbol_bwd_translator;
	EntropyTranslator entropy_translator;

	bool use_heuristic_DC = false;
	bool use_heuristic_bdepthAc = false;

	std::array<symbolstream, 3> vlwstreams{
		symbolstream(generate_vector_with_capacity<dense_vlw_t::type>(144)),
		symbolstream(generate_vector_with_capacity<dense_vlw_t::type>(128)),
		symbolstream(generate_vector_with_capacity<dense_vlw_t::type>(128))
	};

public:
	template <typename obwT>
	void encode(segment<T>& input, obitwrapper<obwT>& output);

	template <typename ibwT>
	void decode(segment<T>& output, ibitwrapper<ibwT>& input);

	void set_use_heuristic_DC(bool value);
	bool get_use_heuristic_DC() const;
	void set_use_heuristic_bdepthAc(bool value);
	bool get_use_heuristic_bdepthAc() const;
private:
	template <typename D, typename obwT>
	void kOptimal(kParams<D> params, obitwrapper<obwT>& output_stream);

	template <typename D, typename obwT>
	void kHeuristic(kParams<D> params, obitwrapper<obwT>& output_stream);

	template <typename D, typename obwT>
	void kReverse(kParams<D>& params, ibitwrapper<obwT>& input_stream);

	template <typename D, typename obwT>
	inline void kEncode(kParams<D> params, bool use_heuristic, obitwrapper<obwT>& output_stream);

	template <typename D, typename obwT>
	inline void kDecode(kParams<D>& params, ibitwrapper<obwT>& input_stream);

	template <typename obwT>
	void encodeBpeStages(segment<T>& input, obitwrapper<obwT>& output);

	template <typename ibwT>
	void decodeBpeStages(segment<T>& output, ibitwrapper<ibwT>& input);
};

// implementation section

// BitPlaneXcoder implementation
template <typename T, size_t alignment>
template <typename obwT>
void BitPlaneXcoder<T, alignment>::encode(segment<T>& input, obitwrapper<obwT>& output) {
	// TODO: check if needed to implement input segment validation (input.size > 1?)

	if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_quant_DC) == 0) {
		kParams quantizedDcParams = {
			input.quantizedDc.data(),
			input.size - 1,	// -1 because of a reference sample that is not part of the vector
			input.bdepthDc - input.q, 
			input.referenceSample
		}; 
		kEncode(quantizedDcParams, this->use_heuristic_DC, output);
	}

	if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_additional_DC) == 0) {
		size_t bit_shift_LL3 = input.bit_shifts[0];
		size_t additional_bdepth = std::max(input.bdepthAc, bit_shift_LL3);
		if (input.q > additional_bdepth) {
			bplaneEncode(input.plainDc.data(), input.size, input.q, input.q - additional_bdepth, output);
		}
	}
	
	if (input.bdepthAc > 0) {
		if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_bdepth_AC) == 0) {
			kParams quantizedBdepthAcParams = {
				input.quantizedBdepthAc.data(),
				input.size - 1,	// -1 because of a reference sample that is not part of the vector
				std::bit_width(input.bdepthAc),
				input.referenceBdepthAc
			};
			kEncode(quantizedBdepthAcParams, this->use_heuristic_bdepthAc, output);
		}
		encodeBpeStages(input, output); // TODO: somehow links without forward declaration
	} // TODO: else {} check if DC data can remain uncoded otherwise

	// TODO: flush output buffer and handle fill bits in the last flashed word
	// TODO: put obitwrapper buffer type parameter size as Header part 4 field
	// CodeWordLength, see 4.2.5.2.8
	// Current output buffer content is flashed in obitwrapper destructor
	// See 4.2.3.2.1, 4.2.3.2.5 and 4.2.5.2.8
	// return bpe_debug_output_buffer.size();
}

template <typename T, size_t alignment>
template <typename D, typename obwT>
void BitPlaneXcoder<T, alignment>::kOptimal(kParams<D> params, obitwrapper<obwT>& output_stream) {
	constexpr size_t max_N_value = 10;
	size_t N = params.bdepth; // N is expected to be in range [1, 10]
	// N is a property of a whole segment. 
	// Therefore k_bound value is the same for every gaggle in the segment
	ptrdiff_t k_bound = ((ptrdiff_t)(N)) - 2;

	// compute lengths for all gaggles for all k encoding parameters;
	// k has at most 9 different values for encoded data (0000 - 1000).
	// max bit length is 2^10 - 1 (max DC value) * 16 (max gaggle length)
	//	+ 16 additional bits (for '1' bit at the end of variable length sequence)
	//	= 16384. That requires 14 bits; 2-byte integer may be used
	// 

	// For implementations with small-width simd instructions computing all 
	// possible stream lengths for all k values may be redundant and may
	// hurt performance; in this case well-predictable if branch is needed.
	//

	constexpr size_t gaggle_len = items_per_gaggle; // refactor?
	constexpr size_t gaggle_scale_factor = 2;

	// zero placehoder for DC reference samble. 
	// zero remaining placeholders for dc elements at the end of the segmnet to extend 
	// the last gaggle to len=16
	{
		params.data[-1] = 0;
		constexpr size_t gaggle_mask = gaggle_len - 1;
		ptrdiff_t dc_boundary = params.datalength + ((~params.datalength) & gaggle_mask);
		for (ptrdiff_t i = params.datalength; i < dc_boundary; ++i) {
			params.data[i] = 0;
		}
	}

	constexpr size_t k_simd_width = 8;
	std::vector<std::pair<size_t, ptrdiff_t>> k_options((params.datalength >> 4) + (gaggle_scale_factor * 2)); // extended just in case

	uint_least16_t simdr_k_bound_mask[k_simd_width] = { 0 };
	for (ptrdiff_t i = k_bound; i < k_simd_width; ++i) {
		// clear sign bit to make comparison operate correctly later
		simdr_k_bound_mask[i] = ((uint_least16_t)((ptrdiff_t)(-1))) >> 1;
	}
	constexpr uint_least16_t simdr_shift[k_simd_width] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	// requires data to have gaggle_len * gaggle_scale_factor additional elements at the end (at most)
	for (ptrdiff_t i = 0; i < params.datalength; i += gaggle_len * gaggle_scale_factor) {
		uint_least16_t simdr_acc[gaggle_scale_factor][k_simd_width] = { 0 };
		uint_fast16_t gpr_acc[gaggle_scale_factor] = { 0 };

		// attempt to increase pipline load uniformity; repeat uniform operation blocks several times
		// should be beneficial for branch prediction in the second block as well
		for (ptrdiff_t j = 0; j < gaggle_scale_factor; ++j) {
			for (ptrdiff_t ii = 0; ii < gaggle_len; ++ii) {
				uint_fast16_t gpr = params.data[i + (j * gaggle_len) + ii - 1]; // -1 here because of reference sample
				gpr_acc[j] += gpr;
				uint_least16_t simdr[k_simd_width] = { 0 };
				for (ptrdiff_t jj = 0; jj < k_simd_width; ++jj) {
					simdr[jj] = gpr;
					simdr[jj] >>= simdr_shift[jj];
					simdr_acc[j][jj] += simdr[jj];
				}
			}
			for (ptrdiff_t jj = 0; jj < k_simd_width; ++jj) {
				simdr_acc[j][jj] |= simdr_k_bound_mask[jj];
			}
		}

		ptrdiff_t k_option_index = i >> 4;
		for (ptrdiff_t j = 0; j < gaggle_scale_factor; ++j) {
			// There is no straitforward way to compare with uncoded option reliably due to different 
			// uncoded stream length for the first gaggle, subsequent gaggles and the last one. 
			// Encoded stream lengths can be compared safely on the other hand as the diff value due to
			// additional '1' bit is the same for all k options for a given gaggle.
			decltype(k_options)::value_type current_k = { gpr_acc[j], 0 };
			// hopefully this can be vectorized or at least well-predicted
			for (ptrdiff_t jj = 0; jj < k_simd_width; ++jj) {
				if (simdr_acc[j][jj] < current_k.first) {
					current_k.first = simdr_acc[j][jj];
					current_k.second = simdr_shift[jj];
				}
			}
			k_options[k_option_index + j] = current_k;
		}

		// PMINSW x86-64: vectorized min for 16-bit words
		// several simd iterations per one loop cycle for 2+ gaggles that lays continiously
		// compute simd bitmask to reflect k_bound to invalidate simd items with index > k_bound (make max signed numbers)
		// 
		// store gpr and min simd k results in a temporal buffer. Buffer length is known before processing
		// (but we need k value ~ index rather then pre-computed length itself)
		// 
	}

	// adjust parameters for the first gaggle (see 4.3.2.8 and 4.3.2.10, figures 4-8 and 4-9)
	{
		size_t k_bitsize = std::bit_width(N - 1);
		constexpr ptrdiff_t k_uncoded_opt = -1;
		// take into account '1' bits inserted into the stream after the first part of the codewords
		// to make it comparable with encoded stream lengths
		size_t k_uncoded_len = N * gaggle_len - gaggle_len;
		size_t k_uncoded_len_first = k_uncoded_len + 1 - N; // TODO: ensure that unsigned arithmetic is not reordered

		ptrdiff_t gaggle_index = 0;
		if (k_options[gaggle_index].first >= k_uncoded_len_first) {
			k_options[gaggle_index].second = k_uncoded_opt;
		}
		output_stream << vlw_t{ k_bitsize, (size_t)(k_options[gaggle_index].second) };
		output_stream << vlw_t{ N, (size_t)(params.reference) };

		ptrdiff_t i;
		bool execute_preamble = false;
		for (i = -1; i < ((ptrdiff_t)(params.datalength - gaggle_len)); i += gaggle_len) {
			if (execute_preamble) {
				gaggle_index = (i >> 4) + 1;
				if (k_options[gaggle_index].first >= k_uncoded_len) {
					k_options[gaggle_index].second = k_uncoded_opt;
				}
				output_stream << vlw_t{ k_bitsize, (size_t)(k_options[gaggle_index].second) };
			}
			execute_preamble = true;

			if (k_options[gaggle_index].second != k_uncoded_opt) {
				for (ptrdiff_t j = (i < 0); j < gaggle_len; ++j) {
					output_stream << vlw_t{ (size_t)((params.data[i + j] >> k_options[gaggle_index].second) + 1), 1 };
				}
			} else {
				k_options[gaggle_index].second = N;
			}

			for (ptrdiff_t j = (i < 0); j < gaggle_len; ++j) {
				output_stream << vlw_t{ (size_t)(k_options[gaggle_index].second), (size_t)(params.data[i + j]) };
			}
		}

		ptrdiff_t last_gaggle_len = params.datalength - i;
		size_t k_uncoded_len_last = N * last_gaggle_len - last_gaggle_len;
		if (execute_preamble) {
			gaggle_index = (i >> 4) + 1;
			if (k_options[gaggle_index].first >= k_uncoded_len_last) {
				k_options[gaggle_index].second = k_uncoded_opt;
			}
			output_stream << vlw_t{ k_bitsize, (size_t)(k_options[gaggle_index].second) };
		}
		execute_preamble = true;

		if (k_options[gaggle_index].second != k_uncoded_opt) {
			for (ptrdiff_t j = (i < 0); j < last_gaggle_len; ++j) {
				output_stream << vlw_t{ (size_t)((params.data[i + j] >> k_options[gaggle_index].second) + 1), 1 };
			}
		} else {
			k_options[gaggle_index].second = N;
		}

		for (ptrdiff_t j = (i < 0); j < last_gaggle_len; ++j) {
			output_stream << vlw_t{ (size_t)(k_options[gaggle_index].second), (size_t)(params.data[i + j]) };
		}
	}
}

template <typename T, size_t alignment>
template <typename D, typename obwT>
void BitPlaneXcoder<T, alignment>::kHeuristic(kParams<D> params, obitwrapper<obwT>& output_stream) {
	size_t N = params.bdepth; // N is expected to be in range [1, 10]
	size_t k_bitsize = std::bit_width(N - 1);
	constexpr ptrdiff_t k_uncoded_option = -1;
	constexpr size_t gaggle_len = 16;
	ptrdiff_t J = 15;
	for (ptrdiff_t i = 0; i < params.datalength; i += J) {
		if (i > 0) {
			J = std::min(gaggle_len, params.datalength - i);
		}
		size_t delta = 0;
		for (ptrdiff_t j = 0; j < J; ++j) {
			delta += params.data[i + j];
		}

		std::array<size_t, 3> predicates1 = {
			delta * 64, 
			J * 207, 
			J * (1 << (N + 5)), 
		};
		std::array<size_t, 3> predicates2 = {
			J * 23 * (1 << N), 
			delta * 128, 
			(delta * 128) + (J * 49), 
		};

		ptrdiff_t k = k_uncoded_option;
		if (predicates1[0] >= predicates2[0]) {
			k = k_uncoded_option;
		} else if (predicates1[1] > predicates2[1]) {
			k = 0;
		} else if (predicates1[2] <= predicates2[2]) {
			k = N - 2;
		} else {
			// TODO: check underflow/arithmetic/input values, potential UB
			k = std::bit_width(predicates2[2] / J) - 7;
		}

		output_stream << vlw_t{ k_bitsize, (size_t)(k) };
		if (i == 0) {
			output_stream << vlw_t{ N, (size_t)(params.reference) };
		}
		if (k != k_uncoded_option) {
			for (ptrdiff_t j = 0; j < J; ++j) {
				output_stream << vlw_t{ (size_t)((params.data[i + j] >> k) + 1), 1 };
			}
		} else {
			k = N;
		}
		for (ptrdiff_t j = 0; j < J; ++j) {
			output_stream << vlw_t{ (size_t)(k), (size_t)(params.data[i + j])};
		}
	}
}

template <typename T, size_t alignment>
template <typename D, typename obwT>
inline void BitPlaneXcoder<T, alignment>::kEncode(kParams<D> params, bool use_heuristic, obitwrapper<obwT>& output_stream) {
	// TODO: check if std::max is needed with 1 as a second parameter for N computation
	// see 4.3.2.1
	params.bdepth = std::max(params.bdepth, (decltype(params.bdepth))(1)); // N is expected to be in range [1, 10]
	if (params.bdepth > 1) {
		if (use_heuristic) {
			kHeuristic(params, output_stream);
		} else {
			kOptimal(params, output_stream);
		}
	} else {
		// N == 1
		output_stream << vlw_t{ params.bdepth, (size_t)(params.reference) };
		for (ptrdiff_t i = 0; i < params.datalength; ++i) {
			output_stream << vlw_t{ params.bdepth, (size_t)(params.data[i]) };
		}
	}
}

template <typename T, size_t alignment>
template <typename obwT>
void BitPlaneXcoder<T, alignment>::encodeBpeStages(segment<T>& input, obitwrapper<obwT>& output) {
	std::vector<uint64_t> block_signs;
	block_signs.reserve(input.size);
	// will be better to extract several bplanes at once, therefore memory 
	// reusage considerations arise. Vector is considered to be not efficient 
	// enough in that scenario, therefore deque is preferred.
	// std::vector<uint64_t> current_bplane(input.size);	// effectively encodes types 0 and 1
	std::deque<uint64_t> current_bplane;	// effectively encodes types 0 and 1
	std::vector<uint64_t> bplane_mask(input.size, 0);		// encodes types 2 and -1
	// basically type -1 takes effect for the entire subband at once, but 
	// the technique with dedicated bplane_skip supports individual type -1
	// encoding for every AC coefficient in the block. Can be alternatively 
	// implemented as a single uint64_t mask applicable to every block in a 
	// segment; the values can be set per the corresponding masks for
	// DC, P1, P2, P3, C1, C2, C3, G1, G2, G3 (that corresponds to DWT subbands)
	// As per the current implementation, the setting will only be checked 
	// during the "tail" bpe processing for bitplanes b < max(subband_weights).
	// 
	// As there's no other predicate applicable to assign a coefficient a type 
	// value -1 but subband weight factor (that applies to the whole subband 
	// and generation), implement bplane_skip as a single-element uint64_t
	// applicable for the whole segment.
	// std::vector<uint64_t> bplane_skip(input.size);		// encodes type -1
	uint64_t bplane_skip = 0;	// encode type -1 for all blocks in the entire segment

	auto block_signs_callback = [&](uint64_t val) -> void {
		block_signs.push_back(val);
	};
	auto current_bplane_callback = [&](uint64_t val) -> void {
		current_bplane.push_back(val);
	};

	obitwrapper<uint64_t> block_signs_bitwrapper(block_signs_callback);
	obitwrapper<uint64_t> current_bitplane_bitwrapper(current_bplane_callback);
	T* raw_block_data = std::assume_aligned<(alignment * sizeof(T))>(input.data.data()->content);
	size_t raw_block_size = input.size * items_per_block;

	{
		// does unnecessary left shifts and cannot perform additional op with data, 
		// we'd like to abs by vectors in addition. Implementation inlined below.
		// maybe add template lambda as a parameter
		// bplane(input.data.data()->content, input.size * items_per_block, (sizeof(T) << 3) - 1, block_signs_bitwrapper);

		constexpr size_t sign_bindex = (sizeof(T) << 3) - 1;
		std::array<std::make_unsigned_t<T>, alignment> masks;
		std::array<T, alignment> rshifts;
		std::array<std::make_unsigned_t<T>, alignment> buffer;
		for (ptrdiff_t i = 0; i < rshifts.size(); ++i) {
			std::make_signed_t<T> relative_shift = ((std::make_signed_t<decltype(sign_bindex)>)(sign_bindex)) - alignment + 1 + i;
			masks[i] = ((size_t)(0x01) << sign_bindex);
			rshifts[i] = relu(relative_shift);
		}

		// TODO: use of implementation details instead of interface below
		std::make_unsigned_t<sufficient_integral_i<(alignment >> 3)>> serial_buffer;

		for (ptrdiff_t i = 0; i < raw_block_size; i += alignment) {
			serial_buffer = 0;
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				buffer[j] = (raw_block_data[i + j] & masks[j]);
				// here is additional payload (vectorized abs)
				raw_block_data[i + j] = magnitude(raw_block_data[i + j]);
			}
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				// TODO: check if promotion needed to integral sufficient to hold shifted value
				// TODO: compile-time checks to prevent the following
				// possible data loss if alignment > (sizeof(T) << 3)
				serial_buffer |= buffer[j] >> rshifts[j];
			}
			block_signs_bitwrapper << vlw_t{ alignment, serial_buffer };
		}
		// end of inlined bplane implementation
		//
	}

	// 38 words total compound block context
	struct block_bpe_meta {
		// TODO: consider splitting the structure into 3 dedicated structs
		// for each relevant encoding stage.

		dense_vlw_t types_P;
		std::array<dense_vlw_t, F_num> types_C;
		std::array<std::array<dense_vlw_t, HxpF_num>, F_num> types_H;

		dense_vlw_t signs_P;
		std::array<dense_vlw_t, F_num> signs_C;
		std::array<std::array<dense_vlw_t, HxpF_num>, F_num> signs_H;

		dense_vlw_t tran_B;
		dense_vlw_t tran_D;
		dense_vlw_t tran_G;
		std::array<dense_vlw_t, F_num> tran_H;

		uint64_t bplane_mask_transit;
	};

	struct gaggle_bpe_meta {
		std::array<ptrdiff_t, 5> entropy_codeoptions = { -1, -1, -1, -1, -1 };
		std::array<bool, 5> code_marker_written = { true, true, false, false, false };
	};

	std::vector<block_bpe_meta> block_states(input.size);
	std::vector<gaggle_bpe_meta> gaggle_states((input.size + items_per_gaggle) >> 4);

	// 21 variable length encoded words per block.
	// 3 words have maximum length = 3
	// 18 words have maximum length = 4
	// 
	// max number of variable len words in the gaggle with length = 4:
	//	18 * 16 = 288
	// max bitlen of variable len words in the gaggle with length = 4:
	//	288 * 4 = 1152
	// 
	// max number of variable len words in the gaggle with length = 3:
	//	3 * 16 = 48
	//	5 * 16 = 80
	//	10 * 16 = 160
	//	16 * 16 = 256
	// max bitlen of variable len words in the gaggle with length = 3:
	//	256 * 3 = 768
	// 
	// max number of variable len words in the gaggle with length = 2:
	//	3 * 16 = 48
	//	8 * 16 = 128
	//	13 * 16 = 208
	//	16 * 16 = 256
	// max bitlen of variable len words in the gaggle with length = 2:
	//	256 * 2 = 512
	//

	auto computeAccGaggleVlw = [&] <bool enable_short_circuit = false> (ptrdiff_t i, size_t gaggle_len) -> void {
		auto& vlwstreams = this->vlwstreams;
		for (ptrdiff_t j = 0; j < gaggle_len; ++j) {
			std::array<typename decltype(family_masks)::value_type, family_masks.size()> prepared_block = { 0 };
			for (ptrdiff_t k = 0; k < family_masks.size(); ++k) {
				prepared_block[k] = current_bplane[i + j] & family_masks[k];
			}

			auto addVlwToCollection = [&](dense_vlw_t& vlw) -> void {
			// auto addVlwToCollection = [&](bpe_variadic_length_word& vlw, size_t vlw_len) -> void {
				constexpr size_t vlwstreams_index_bias = 2;
				size_t vlw_len = vlw.length;
				// 
				// PERFMARK:	TODO:
				// Keep branch. If lambda's operator() gets inlined, then there should be
				// enough instruction queue clocks to elide branch prediction. 
				// // Hence explicit
				// // function parameter for word length to avoid transitive dependincies through
				// // vlw structure.
				//
				if (vlw_len >= 2) {
					vlwstreams[vlw_len - vlwstreams_index_bias].put(vlw.value);
				}
			};

			auto accumulateTranVlw = [&](dense_vlw_t& vlw, size_t index, bool skip = false) -> bool {
				// Accumulates transition word with next signle bit. Since the word
				// is not complete yet, the correspnding stream for length estimation
				// is not populated, therefore the resulting symbol should be inserted
				// into the stream when the complete word is ready.
				//

				size_t tran_val = prepared_block[index] > 0;
				size_t tran_len = ((bplane_mask[i + j] & family_masks[index]) == 0);
				tran_len &= !skip;
				if constexpr (enable_short_circuit) {
					tran_len &= ((bplane_skip & family_masks[index]) != family_masks[index]);
				}

				// tran_len value is either 0 or 1, and used as both mask and len
				vlw.value <<= tran_len;
				vlw.value |= (tran_len & tran_val);
				vlw.length += tran_len;

				// skip is transitive (e.g. tran_D -> tran_G -> tran_Hx)
				return (((tran_val == 0) & (tran_len == 1)) | skip);
			};

			auto computeTerminalVlw = [&](dense_vlw_t& typesVlw, dense_vlw_t& signsVlw, size_t index, bool skip = false) {
				// All active elements that are not of type 2 (or -1) yet, mask is applicable
				// to get values '0' and '1' of dedicated bits
				uint64_t effective_mask = (family_masks[index] & (~bplane_mask[i + j])) & (-((int64_t)(!skip)));
				if constexpr (enable_short_circuit) {
					effective_mask &= ~bplane_skip;
				}

				typesVlw = combine_bword(prepared_block[index], effective_mask);
				// Actually the same as below, but may relief contention for a single item 
				// in current_bplane[i + j] and use masked copy instead, that should be just
				// used by computing transition word
				// 
				// ) = combine_bword(current_bplane[i + j], effective_mask);
				signsVlw = combine_bword(block_signs[i + j], (prepared_block[index] & effective_mask));

				block_states[i + j].bplane_mask_transit |= (prepared_block[index] & effective_mask);
			};

			// set tran words to completely clean states to mitigate interfering with leftovers
			// of the previous bitplane. Also allows write operations only with no need to read
			// the old state and elide assosiated memory ops dependencies.
			block_states[i + j].tran_B = dense_vlw_t{ 0, 0 };
			bool skip_descendants = accumulateTranVlw(block_states[i + j].tran_B, B_index);
			// subband scaling for P coeffs is handled in computeTerminalVlw
			computeTerminalVlw(block_states[i + j].types_P, block_states[i + j].signs_P, P_index);
			symbol_fwd_translator.translate(block_states[i + j].types_P);
			addVlwToCollection(block_states[i + j].types_P);

			if (!skip_descendants) {
				block_states[i + j].tran_D = dense_vlw_t{ 0, 0 };
				block_states[i + j].tran_G = dense_vlw_t{ 0, 0 };
				for (ptrdiff_t k = 0; k < F_num; ++k) {
					// (skip = false) is implied because the case is covered by control flow dependency
					// introduced by branch few lines above
					bool skip_tran_Gx = accumulateTranVlw(block_states[i + j].tran_D, (k * F_step) + D_offset);
					bool skip_tran_Hx = accumulateTranVlw(block_states[i + j].tran_G, (k * F_step) + G_offset, skip_tran_Gx);

					// check bplane_skip here for child coeffs
					bool skip_children = false;
					if constexpr (enable_short_circuit) {
						skip_children = !((bplane_skip & family_masks[(k * F_step) + C_offset]) == 0);
					}
					if (!skip_children) {
						computeTerminalVlw(block_states[i + j].types_C[k], block_states[i + j].signs_C[k],
							(k * F_step) + C_offset, skip_tran_Gx);
						symbol_fwd_translator.translate(block_states[i + j].types_C[k]);
						addVlwToCollection(block_states[i + j].types_C[k]);
					} else {
						block_states[i + j].types_C[k] = dense_vlw_t{ 0, 0 };
						block_states[i + j].signs_C[k] = dense_vlw_t{ 0, 0 };
					}

					block_states[i + j].tran_H[k] = dense_vlw_t{ 0, 0 };
					// check bplane_skip here for grandchildren coeffs
					bool skip_grandchildren = false;
					if constexpr (enable_short_circuit) {
						skip_grandchildren = !((bplane_skip & family_masks[(k * F_step) + G_offset]) == 0);
					}
					if (!skip_grandchildren) {
						for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
							bool skip_Hx = accumulateTranVlw(block_states[i + j].tran_H[k], (k * F_step) + Hx_offset + l, skip_tran_Hx);
							computeTerminalVlw(block_states[i + j].types_H[k][l], block_states[i + j].signs_H[k][l],
								(k * F_step) + Hx_offset + l, skip_Hx);
							symbol_fwd_translator.translate<decltype(symbol_fwd_translator)::types_H_codeparam>(
								block_states[i + j].types_H[k][l]);
							addVlwToCollection(block_states[i + j].types_H[k][l]);
						}
					} else {
						for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
							block_states[i + j].types_H[k][l] = dense_vlw_t{ 0, 0 };
							block_states[i + j].signs_H[k][l] = dense_vlw_t{ 0, 0 };
						}
					}
				}
			}

			symbol_fwd_translator.translate<decltype(symbol_fwd_translator)::tran_D_codeparam>(block_states[i + j].tran_D);
			addVlwToCollection(block_states[i + j].tran_D);
			symbol_fwd_translator.translate(block_states[i + j].tran_G);
			addVlwToCollection(block_states[i + j].tran_G);
			for (size_t k = 0; k < block_states[i + j].tran_H.size(); ++k) {
				symbol_fwd_translator.translate<decltype(symbol_fwd_translator)::tran_H_codeparam>(block_states[i + j].tran_H[k]);
				addVlwToCollection(block_states[i + j].tran_H[k]);
			}
		}
	};

	auto computeGaggleParams = [&](ptrdiff_t gaggle_index) -> void {
		std::array<std::pair<size_t, ptrdiff_t>, std::tuple_size_v<decltype(vlwstreams)>> optimal_code_options = { {
			{ vlwstreams[0]._size() * 2, -1 },
			{ vlwstreams[1]._size() * 3, -1 },
			{ vlwstreams[2]._size() * 4, -1 }
		} };

		// TODO: naming consistence, camel or underscores
		auto update_optimal_code_option = [&](size_t index, size_t blen, size_t option) -> void {
			size_t current_bitsize = 0;
			while (vlwstreams[index]) {
				current_bitsize += entropy_translator.word_length(vlwstreams[index].get(), blen, option);
			}
			if (current_bitsize < optimal_code_options[index].first) {
				optimal_code_options[index] = { current_bitsize, option };
			}
			vlwstreams[index].restart();
		};

		update_optimal_code_option(0, 2, 0);
		update_optimal_code_option(1, 3, 0);
		update_optimal_code_option(1, 3, 1);
		update_optimal_code_option(2, 4, 0);
		update_optimal_code_option(2, 4, 1);
		update_optimal_code_option(2, 4, 2);

		for (ptrdiff_t k = 0; k < vlwstreams.size(); ++k) {
			constexpr size_t codeoptions_bias = 2;
			gaggle_states[gaggle_index].entropy_codeoptions[k + codeoptions_bias] = optimal_code_options[k].second;
			gaggle_states[gaggle_index].code_marker_written[k + codeoptions_bias] = false;
			vlwstreams[k].reset();
		}
	};
	
	auto translateEncodeVlw = [&](dense_vlw_t& vlw, ptrdiff_t gaggle_index) -> void {
		size_t vlw_length = vlw.length;
		ptrdiff_t codeoption = gaggle_states[gaggle_index].entropy_codeoptions[vlw_length];
		if (!gaggle_states[gaggle_index].code_marker_written[vlw_length]) {
			// for word length 0 and 1 predicate is always false, should
			// never be executed for these lengths
			gaggle_states[gaggle_index].code_marker_written[vlw_length] = true;
			size_t codeoption_bitsize = std::bit_width((size_t)(vlw_length - 1)); // TODO: refactor type casts
			output << vlw_t{ codeoption_bitsize, (vlw_t::type)(codeoption) };
		}
		output << entropy_translator.translate(vlw, codeoption);
	};

	constexpr size_t gaggle_mask = items_per_gaggle - 1; // 0x0f
	size_t input_size_truncated = input.size & (~gaggle_mask);
	size_t last_gaggle_size = input.size & gaggle_mask;

	size_t b = input.bdepthAc - 1; // TODO: requires bdepthAc > 0; check at caller side
	for (; b >= max_subband_shift; --b) {
		if (current_bplane.empty()) {
			// Per the loop conditions b is guaranteed to be more than or equal to 3, 
			// that means at least 4 bitplanes are available always. 
			size_t bplane_num_to_extract = 4;
			// fills current_bplane container
			bplaneEncode(raw_block_data, raw_block_size, b, bplane_num_to_extract, current_bitplane_bitwrapper);
		}

		// stage 0
		if (b < input.q) {
			// q may be less than bdepthAc, e.g.:
			// bdepthAc = 3; bdepthDc = 10; bitShift(LL3) = 0
			// => 
			// q = 2
			//
			if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_0) == 0) {
				bplaneEncode(input.plainDc.data(), input.size, b, 1, output); // the output buffer can still not be flushed yet
			}
		}

		// interleaving bpe stages
		{
			// the words not encoded further:
			// (all computed on a dedicated signs data set)
			// signs_P
			// signs_C0
			// signs_C1
			// signs_C2
			// signs_H00
			// signs_H01
			// signs_H02
			// signs_H03
			// signs_H10
			// signs_H11
			// signs_H12
			// signs_H13
			// signs_H20
			// signs_H21
			// signs_H22
			// signs_H23
			//
			// the words encoded further:
			// types_P
			// types_C0
			// types_C1
			// types_C2
			// types_H00
			// types_H01
			// types_H02
			// types_H03
			// types_H10
			// types_H11
			// types_H12
			// types_H13
			// types_H20
			// types_H21
			// types_H22
			// types_H23
			//
			// transition words encoded further:
			// tran_B	// but actually max len is 1, therefore never encoded. But still suits this set
			// tran_D
			// tran_G
			// tran_H0
			// tran_H1
			// tran_H2
			//

			ptrdiff_t i = 0;
			for (; i < input_size_truncated; i += items_per_gaggle) {
				computeAccGaggleVlw(i, items_per_gaggle);
				computeGaggleParams(i >> 4);
			}
			if (last_gaggle_size > 0) {
				computeAccGaggleVlw(i, last_gaggle_size);
				computeGaggleParams(i >> 4);
			}
			
			// The code below writes the computed words to the output stream. Per 4.5.3.1.7 and 
			// 4.5.3.1.8 some words shall not be written to the output stream upon certain 
			// conditions dependent on other words' states. The stages encoding code section 
			// below ignores these dependencies as they are already taken into accoung when 
			// the words are computed (code section above), and all the words that shall not 
			// be written to the stream have length 0, that is, these words are null-words. 
			// All the words are initialized with length 0 in the beginning, and then are 
			// eventually computed when necessary on a later bitplane. Once computed, they're 
			// computed in a subsequent bitplanes also, until evaluate to null-word in some 
			// less signigicant bitplane.
			//

			// stage 1
			if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_1) == 0) {
				i = 0;
				for (; i < input_size_truncated; i += items_per_gaggle) {
					for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
						translateEncodeVlw(block_states[i + j].types_P, (i >> 4));
						output << block_states[i + j].signs_P;
					}
				}
				for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
					translateEncodeVlw(block_states[i + j].types_P, (i >> 4));
					output << block_states[i + j].signs_P;
				}
			}
			
			// stage 2
			if constexpr (((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) || 
					((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_3) == 0)) {
				i = 0;
				for (; i < input_size_truncated; i += items_per_gaggle) {
					for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
						output << block_states[i + j].tran_B;
						translateEncodeVlw(block_states[i + j].tran_D, (i >> 4));
						if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
							for (ptrdiff_t k = 0; k < F_num; ++k) {
								translateEncodeVlw(block_states[i + j].types_C[k], (i >> 4));
								output << block_states[i + j].signs_C[k];
							}
						}
					}
				}
				for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
					output << block_states[i + j].tran_B;
					translateEncodeVlw(block_states[i + j].tran_D, (i >> 4));
					if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
						for (ptrdiff_t k = 0; k < F_num; ++k) {
							translateEncodeVlw(block_states[i + j].types_C[k], (i >> 4));
							output << block_states[i + j].signs_C[k];
						}
					}
				}
			}

			// stage 3
			if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_3) == 0) {
				i = 0;
				for (; i < input_size_truncated; i += items_per_gaggle) {
					for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
						translateEncodeVlw(block_states[i + j].tran_G, (i >> 4));
						for (ptrdiff_t k = 0; k < F_num; ++k) {
							translateEncodeVlw(block_states[i + j].tran_H[k], (i >> 4));
						}
						for (ptrdiff_t k = 0; k < F_num; ++k) {
							for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
								translateEncodeVlw(block_states[i + j].types_H[k][l], (i >> 4));
								output << block_states[i + j].signs_H[k][l];
							}
						}
					}
				}
				for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
					translateEncodeVlw(block_states[i + j].tran_G, (i >> 4));
					for (ptrdiff_t k = 0; k < F_num; ++k) {
						translateEncodeVlw(block_states[i + j].tran_H[k], (i >> 4));
					}
					for (ptrdiff_t k = 0; k < F_num; ++k) {
						for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
							translateEncodeVlw(block_states[i + j].types_H[k][l], (i >> 4));
							output << block_states[i + j].signs_H[k][l];
						}
					}
				}
			}

			// stage 4
			i = 0; // not really necessary, just for debugging consistency
			for (ptrdiff_t j = 0; j < input.size; ++j) {
				if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_4) == 0) {
					output << combine_bword(current_bplane[j], bplane_mask[j]);
				}
				bplane_mask[j] |= block_states[j].bplane_mask_transit;
				block_states[j].bplane_mask_transit = 0;
			}
		}

		{
			// fast forward to the next bplane, will make the container empty
			// if the current bplane was the last extracted.
			// Since the elements at the beginning only are erased, no moves/copies
			// occur and no iteraters are invalidated (as guaranteed by std::deque)

			current_bplane.erase(current_bplane.begin(), current_bplane.begin() + input.size);
		}
	}

	// Duplicates the loop above, but implements scaling handling. The implementatino 
	// below introduces additional control dependencies intending to skip unnecessary
	// computations where applicable.
	bool next_bplane = false;
	do {
		next_bplane = (b != 0);

		for (ptrdiff_t i = 0; i < input.bit_shifts.size(); ++i) {
			// PERF NOTE: will gt be predicted better than eq? Just curious
			// if (input.bit_shifts[i] > b) {
			if (input.bit_shifts[i] == (b + 1)) {
				bplane_skip |= bitshift_masks[i];
			}
		}

		if (current_bplane.empty()) {
			// Per bitplane loop conditions, b is guaranteed to be less than 3. Drain
			// the data up to the end.
			size_t bplane_num_to_extract = b + 1;
			// fills current_bplane container
			bplaneEncode(raw_block_data, raw_block_size, b, bplane_num_to_extract, current_bitplane_bitwrapper);
		}

		// stage 0
		// 
		// As per 4.3.1.2 and 4.3.1.3, q and BitShift LL3 can evaluate to mutually independent values
		// with respect to the lower bound, therefore both checks should be performed; no skip is 
		// possible due to a priori knowledge.
		// 
		if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_0) == 0) {
			if (((bplane_skip & family_masks[DC_index]) == 0) & (b < input.q)) {
				bplaneEncode(input.plainDc.data(), input.size, b, 1, output);
			}
		}

		ptrdiff_t i = 0;
		for (; i < input_size_truncated; i += items_per_gaggle) {
			computeAccGaggleVlw.template operator()<true>(i, items_per_gaggle);
			computeGaggleParams(i >> 4);
		}
		if (last_gaggle_size > 0) {
			computeAccGaggleVlw.template operator()<true>(i, last_gaggle_size);
			computeGaggleParams(i >> 4);
		}

		// stage 1
		// 
		// Check for complex bplane_skip field consisting of 3 independent single bit 
		// masks for parent coeffs, therefore check for unequality instead of eq 0.
		//
		if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_1) == 0) {
			if ((bplane_skip & family_masks[P_index]) != family_masks[P_index]) {
				i = 0;
				for (; i < input_size_truncated; i += items_per_gaggle) {
					for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
						translateEncodeVlw(block_states[i + j].types_P, (i >> 4));
						output << block_states[i + j].signs_P;
					}
				}
				for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
					translateEncodeVlw(block_states[i + j].types_P, (i >> 4));
					output << block_states[i + j].signs_P;
				}
			}
		}

		// stage 2
		// 
		// bplane_skip is not checked during stage 2 encoding due to non-clear 
		// profit and potential performance degradation because of poor branch 
		// prediction. All words that must be skiped due to subband scaling are
		// already zeroed.
		//
		if constexpr (((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) || 
				((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_3) == 0)) {
			i = 0;
			for (; i < input_size_truncated; i += items_per_gaggle) {
				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
					output << block_states[i + j].tran_B;
					translateEncodeVlw(block_states[i + j].tran_D, (i >> 4));
					if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
						for (ptrdiff_t k = 0; k < F_num; ++k) {
							translateEncodeVlw(block_states[i + j].types_C[k], (i >> 4));
							output << block_states[i + j].signs_C[k];
						}
					}
				}
			}
			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
				output << block_states[i + j].tran_B;
				translateEncodeVlw(block_states[i + j].tran_D, (i >> 4));
				if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
					for (ptrdiff_t k = 0; k < F_num; ++k) {
						translateEncodeVlw(block_states[i + j].types_C[k], (i >> 4));
						output << block_states[i + j].signs_C[k];
					}
				}
			}
		}

		// stage 3
		if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_3) == 0) {
			i = 0;
			for (; i < input_size_truncated; i += items_per_gaggle) {
				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
					translateEncodeVlw(block_states[i + j].tran_G, (i >> 4)); // TODO: respect bplane_skip here
					for (ptrdiff_t k = 0; k < F_num; ++k) {
						if ((bplane_skip & family_masks[(k * F_step) + G_offset]) == 0) {
							translateEncodeVlw(block_states[i + j].tran_H[k], (i >> 4));
						}
					}
					for (ptrdiff_t k = 0; k < F_num; ++k) {
						if ((bplane_skip & family_masks[(k * F_step) + G_offset]) == 0) {
							for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
								translateEncodeVlw(block_states[i + j].types_H[k][l], (i >> 4));
								output << block_states[i + j].signs_H[k][l];
							}
						}
					}
				}
			}
			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
				translateEncodeVlw(block_states[i + j].tran_G, (i >> 4));
				for (ptrdiff_t k = 0; k < F_num; ++k) {
					if ((bplane_skip & family_masks[(k * F_step) + G_offset]) == 0) {
						translateEncodeVlw(block_states[i + j].tran_H[k], (i >> 4));
					}
				}
				for (ptrdiff_t k = 0; k < F_num; ++k) {
					if ((bplane_skip & family_masks[(k * F_step) + G_offset]) == 0) {
						for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
							translateEncodeVlw(block_states[i + j].types_H[k][l], (i >> 4));
							output << block_states[i + j].signs_H[k][l];
						}
					}
				}
			}
		}

		// stage 4
		i = 0; // not really necessary, just for convinience during debugging
		for (ptrdiff_t j = 0; j < input.size; ++j) {
			if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_4) == 0) {
				output << combine_bword(current_bplane[j], (bplane_mask[j] & (~bplane_skip)));
			}
			bplane_mask[j] |= block_states[j].bplane_mask_transit;
			block_states[j].bplane_mask_transit = 0;
		}

		current_bplane.erase(current_bplane.begin(), current_bplane.begin() + input.size);
		--b;	// formally UB
	} while (next_bplane);
}

template <typename T, size_t alignment>
template <typename ibwT>
void BitPlaneXcoder<T, alignment>::decode(segment<T>& output, ibitwrapper<ibwT>& input) {
	// segment initalization
	output.data = decltype(output.data)(output.size);
	output.referenceSample = 0;
	output.quantizedDc = aligned_vector<T>(output.size);
	output.plainDc = aligned_vector<T>(output.size);
	output.plainDc.assign(0);
	output.referenceBdepthAc = 0;
	output.quantizedBdepthAc = aligned_vector<size_t>(output.size);
	// output.bdepthAcBlocks = aligned_vector<size_t>(output.size);

	output.q = quant_dc(output.bdepthDc, output.bdepthAc, output.bit_shifts[0]);

	if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_quant_DC) == 0) {
		kParams quantizedDcParams = {
			output.quantizedDc.data(),
			output.size - 1,	// -1 because of a reference sample that is not part of the vector
			output.bdepthDc - output.q,
			output.referenceSample // TODO: put 0 instead
		};
		kDecode(quantizedDcParams, input);
		output.referenceSample = quantizedDcParams.reference;
	}

	if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_additional_DC) == 0) {
		size_t bit_shift_LL3 = output.bit_shifts[0];
		size_t additional_bdepth = std::max(output.bdepthAc, bit_shift_LL3);
		if (output.q > additional_bdepth) {
			bplaneDecode(output.plainDc.data(), output.size, output.q - additional_bdepth, input);
		}
	}

	if (output.bdepthAc > 0) {
		if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_bdepth_AC) == 0) {
			kParams quantizedBdepthAcParams = {
				output.quantizedBdepthAc.data(),
				output.size - 1,	// -1 because of a reference sample that is not part of the vector
				std::bit_width(output.bdepthAc),
				output.referenceBdepthAc // TODO: put 0 instead
			};
			kDecode(quantizedBdepthAcParams, input);
		}
		decodeBpeStages(output, input);
	}
}

template <typename T, size_t alignment>
template <typename D, typename obwT>
void BitPlaneXcoder<T, alignment>::kReverse(kParams<D>& params, ibitwrapper<obwT>& input_stream) {
	size_t N = params.bdepth; // N is expected to be in range [1, 10]
	constexpr size_t gaggle_len = items_per_gaggle; // refactor?

	// adjust parameters for the first gaggle (see 4.3.2.8 and 4.3.2.10, figures 4-8 and 4-9)
	{
		size_t k_bitsize = std::bit_width(N - 1);
		size_t k_uncoded_opt = ~(((ptrdiff_t)(-1)) << k_bitsize);
		size_t current_k_opt = input_stream.extract(k_bitsize);
		params.reference = signext(input_stream.extract(N), N);

		ptrdiff_t i;
		bool current_uncoded = false;
		bool execute_preamble = false;
		for (i = -1; i < ((ptrdiff_t)(params.datalength - gaggle_len)); i += gaggle_len) {
			if (execute_preamble) {
				current_k_opt = input_stream.extract(k_bitsize);
			}
			execute_preamble = true;

			current_uncoded = (current_k_opt == k_uncoded_opt);
			if (!current_uncoded) {
				for (ptrdiff_t j = (i < 0); j < gaggle_len; ++j) {
					params.data[i + j] = input_stream.extract_next_one();
				}
			} else {
				current_k_opt = N;
			}

			for (ptrdiff_t j = (i < 0); j < gaggle_len; ++j) {
				// params.data contains unsigned items
				vlw_t current_item{ 
					current_k_opt, 
					(size_t)(params.data[i + j]) & (-(!current_uncoded))
				};
				input_stream >> current_item;
				params.data[i + j] = current_item.value;
			}
		}

		ptrdiff_t last_gaggle_len = params.datalength - i;
		if (execute_preamble) {
			current_k_opt = input_stream.extract(k_bitsize);
		}

		current_uncoded = (current_k_opt == k_uncoded_opt);
		if (current_k_opt != k_uncoded_opt) {
			for (ptrdiff_t j = (i < 0); j < last_gaggle_len; ++j) {
				params.data[i + j] = input_stream.extract_next_one();
			}
		} else {
			current_k_opt = N;
		}

		for (ptrdiff_t j = (i < 0); j < last_gaggle_len; ++j) {
			vlw_t current_item{ 
				current_k_opt, 
				(size_t)(params.data[i + j]) & (-(!current_uncoded))
			};
			input_stream >> current_item;
			params.data[i + j] = current_item.value;
		}
	}
}

template <typename T, size_t alignment>
template <typename D, typename obwT>
inline void BitPlaneXcoder<T, alignment>::kDecode(kParams<D>& params, ibitwrapper<obwT>& input_stream) {
	// TODO: check if std::max is needed with 1 as a second parameter for N computation
	// see 4.3.2.1
	params.bdepth = std::max(params.bdepth, (decltype(params.bdepth))(1)); // N is expected to be in range [1, 10]
	if (params.bdepth > 1) {
		kReverse(params, input_stream);
	} else {
		// N == 1
		params.reference = signext(input_stream.extract(params.bdepth), params.bdepth);
		for (ptrdiff_t i = 0; i < params.datalength; ++i) {
			params.data[i] = signext(input_stream.extract(params.bdepth), params.bdepth);
		}
	}
}

template <typename T, size_t alignment>
template <typename ibwT>
void BitPlaneXcoder<T, alignment>::decodeBpeStages(segment<T>& output, ibitwrapper<ibwT>& input) {
	// std::vector<uint64_t> current_bplane(output.size, 0);
	// std::vector<uint64_t> block_signs(output.size, 0);
	aligned_vector<uint64_t> current_bplane(output.size);
	current_bplane.assign(0);
	aligned_vector<uint64_t> block_signs(output.size);
	block_signs.assign(0);
	std::vector<uint64_t> bplane_mask(output.size, 0);
	uint64_t bplane_skip = 0;
	
	struct block_bpe_meta {
		dense_vlw_t tran_B = { 1, 0 };
		std::array<dense_vlw_t, F_num> tran_D = { { 
			{ 1, 0 }, { 1, 0 }, { 1, 0 } 
		} };
		std::array<dense_vlw_t, F_num> tran_G = { { 
			{ 1, 0 }, { 1, 0 }, { 1, 0 } 
		} };
		std::array<std::array<dense_vlw_t, HxpF_num>, F_num> tran_Hx = { {
			{{ { 1, 0 }, { 1, 0 }, { 1, 0 }, { 1, 0 } }},
			{{ { 1, 0 }, { 1, 0 }, { 1, 0 }, { 1, 0 } }},
			{{ { 1, 0 }, { 1, 0 }, { 1, 0 }, { 1, 0 } }}
		} };

		uint64_t bplane_mask_transit;
	};

	struct gaggle_bpe_meta {
		std::array<ptrdiff_t, 5> entropy_codeoptions = { -1, -1, -1, -1, -1 };
		std::array<bool, 5> code_marker_processed = { true, true, false, false, false };
	};

	std::vector<block_bpe_meta> block_states(output.size);
	std::vector<gaggle_bpe_meta> gaggle_states((output.size + items_per_gaggle) >> 4);

	auto decodeTranslateVlw = [&](size_t vlw_length, size_t gaggle_index) -> vlw_t {
		ptrdiff_t codeoption = gaggle_states[gaggle_index].entropy_codeoptions[vlw_length];
		if (gaggle_states[gaggle_index].code_marker_processed[vlw_length] == false) {
			gaggle_states[gaggle_index].code_marker_processed[vlw_length] = true;
			size_t codeoption_bitsize = std::bit_width(vlw_length - 1);
			ptrdiff_t uncoded_option = ~((-1) << codeoption_bitsize);
			codeoption = input.extract(codeoption_bitsize);
			if (codeoption == uncoded_option) {
				codeoption = signext(codeoption, codeoption_bitsize);
			}
			gaggle_states[gaggle_index].entropy_codeoptions[vlw_length] = codeoption;
		}
		return entropy_translator.extract(input, vlw_length, codeoption);
	};

	auto decodeTypesSigns = [&]<size_t codeoption = 0>(ptrdiff_t gaggle_index, ptrdiff_t block_index, uint64_t mask) -> void {
		ptrdiff_t i = gaggle_index << 4;
		ptrdiff_t j = block_index;
		size_t bitsize_types = std::popcount(mask);
		vlw_t types = decodeTranslateVlw(bitsize_types, gaggle_index);
		symbol_bwd_translator.translate<codeoption>(types);
		size_t bitsize_signs = std::popcount(types.value);
		uint64_t signs = input.extract(bitsize_signs);
		uint64_t extracted_types = decompose_bword(types.value, mask);
		uint64_t extracted_signs = decompose_bword(signs, extracted_types);
		block_signs[i + j] |= extracted_signs;
		block_states[i + j].bplane_mask_transit |= extracted_types;
		current_bplane[i + j] |= extracted_types;
	};

	auto decodeTran = [&]<size_t codeoption = 0>(auto & tran, ptrdiff_t gaggle_index, size_t dependency_mask = 0x0ff) -> void {
		// size_t tran_length = 0;
		// // The loop below sets implicit requirement on tran to be collection (provide begin() and end())
		// // and have the contained type convertible to dense_vlw_t (that is, _vlw_t)
		// for (dense_vlw_t item : tran) {
		// 	tran_length += item.length;
		// }
		// 
		// vlw_t tran_raw = decodeTranslateVlw(tran_length, gaggle_index);
		// symbol_bwd_translator.translate<codeoption>(tran_raw);
		// 
		// // for (ptrdiff_t k = std::tuple_size_v<decltype(tran)>> - 1; k >= 0; --k) {
		// for (ptrdiff_t k = tran.size() - 1; k >= 0; --k) {
		// 	tran[k].value = (tran_raw.value & 0x01) & tran[k].length;
		// 	tran_raw.value >>= tran[k].length;
		// 	tran[k].length &= (~tran[k].value);
		// }

		size_t tran_mask = 0;
		// The loop below sets implicit requirement on tran to be collection (provide begin() and end())
		// and have the contained type convertible to dense_vlw_t (that is, _vlw_t)
		for (dense_vlw_t item : tran) {
			tran_mask <<= 1;
			tran_mask |= item.length;
		}
		tran_mask &= dependency_mask;

		vlw_t tran_raw = decodeTranslateVlw(std::popcount(tran_mask), gaggle_index);
		symbol_bwd_translator.translate<codeoption>(tran_raw);

		// for (ptrdiff_t k = std::tuple_size_v<decltype(tran)>> - 1; k >= 0; --k) {
		for (ptrdiff_t k = tran.size() - 1; k >= 0; --k) {
			size_t effective_length = (tran_mask & 0x01); // &tran[k].length;
			tran[k].value = (tran_raw.value & 0x01) & effective_length;
			tran_raw.value >>= effective_length;
			tran[k].length &= (~tran[k].value);
			tran_mask >>= 1;
		}
	};

	constexpr size_t gaggle_mask = items_per_gaggle - 1; // 0x0f
	size_t last_gaggle_start = output.size & (~gaggle_mask);
	size_t last_gaggle_size = output.size & gaggle_mask;

	ptrdiff_t b = output.bdepthAc;
	while (b > 0) {
		for (ptrdiff_t i = 0; i < output.bit_shifts.size(); ++i) {
			if (output.bit_shifts[i] == b) {
				bplane_skip |= bitshift_masks[i];
			}
		}

		// stage 0
		if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_0) == 0) {
			if ((b <= output.q) & ((bplane_skip & family_masks[DC_index]) == 0)) {
				bplaneDecode(output.plainDc.data(), output.size, 1, input);
			}
			// skip DC handling otherwise for this bitplane and pass the decoded
			// coefficients to DWT with no scaling performed; the coefficients 
			// are already unscaled naturally and passed in a separate buffer
		}

		// stage 1
		ptrdiff_t i = 0;
		for (; i < output.size; i += items_per_gaggle) {
			// reset gaggle states here
			gaggle_states[i >> 4].code_marker_processed = { true, true, false, false, false };
			if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_1) == 0) {
				if ((bplane_skip & family_masks[P_index]) != family_masks[P_index]) {
					size_t current_gaggle_size = (i == last_gaggle_start) ? last_gaggle_size : items_per_gaggle;
					for (ptrdiff_t j = 0; j < current_gaggle_size; ++j) {
						uint64_t types_P_extract_mask = family_masks[P_index] & (~bplane_mask[i + j]) & (~bplane_skip);
						decodeTypesSigns((i >> 4), j, types_P_extract_mask);
					}
				}
			}
		}
		
		// stage 2
		if constexpr (((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) || 
				((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_3) == 0)) {
			i = 0;
			for (; i < output.size; i += items_per_gaggle) {
				size_t current_gaggle_size = (i == last_gaggle_start) ? last_gaggle_size : items_per_gaggle;
				for (ptrdiff_t j = 0; j < current_gaggle_size; ++j) {
					dense_vlw_t& tran_B = block_states[i + j].tran_B;
					tran_B.length &= ((bplane_skip & family_masks[B_index]) != family_masks[B_index]);
					tran_B.value = input.extract(tran_B.length);
					tran_B.length &= (~tran_B.value);
					if (tran_B.length == 0) {
						auto& tran_D = block_states[i + j].tran_D;
						// TODO: so the code below became not specific to tran_G only.
						// Even for tran_B check for bplane_skip is performed.
						// Maybe wrap to a lambda?
						//
						size_t tran_D_dependency_mask = 0;
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							tran_D_dependency_mask <<= 1;
							// TODO: refactor predicate
							tran_D_dependency_mask |= ((bplane_skip & family_masks[(f * F_step) + D_offset]) != family_masks[(f * F_step) + D_offset]);
						}
						tran_D_dependency_mask = ~tran_D_dependency_mask;
						decodeTran.template operator()<SymbolBackwardTranslator::tran_D_codeparam>(tran_D, (i >> 4), tran_D_dependency_mask);
						decodeTran.template operator()<SymbolBackwardTranslator::tran_D_codeparam>(tran_D, (i >> 4));

						if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
							// TODO: loop variables naming consistense
							for (ptrdiff_t f = 0; f < F_num; ++f) {
								if ((tran_D[f].length == 0) & ((bplane_skip & family_masks[(f * F_step) + C_offset]) == 0)) {
									uint64_t types_C_extract_mask = family_masks[(f * F_step) + C_offset] & (~bplane_mask[i + j]);
									decodeTypesSigns((i >> 4), j, types_C_extract_mask);
								}
							}
						}
					}
				}
			}
		}

		// stage 3
		if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_3) == 0) {
			i = 0;
			for (; i < output.size; i += items_per_gaggle) {
				size_t current_gaggle_size = (i == last_gaggle_start) ? last_gaggle_size : items_per_gaggle;
				for (ptrdiff_t j = 0; j < current_gaggle_size; ++j) {
					if (block_states[i + j].tran_B.length == 0) {
						auto& tran_D = block_states[i + j].tran_D;
						size_t tran_G_dependency_mask = 0;
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							tran_G_dependency_mask <<= 1;
							// bplane_skip handling below works fine because grandhildren 
							// coeffs correspond to one specific subband, different from 
							// handling bplane_skip for composite tran words.
							//
							tran_G_dependency_mask |= tran_D[f].length & ((bplane_skip & family_masks[(f * F_step) + G_offset]) == 0);
							// tran_G_dependency_mask |= tran_D[f].length;
						}
						tran_G_dependency_mask = ~tran_G_dependency_mask;

						auto& tran_G = block_states[i + j].tran_G;
						decodeTran(tran_G, (i >> 4), tran_G_dependency_mask);
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							// TODO: seems like bplane_skip check below is redundant 
							// already because handled properly when computing tran_G
							if ((tran_G[f].length == 0) & ((bplane_skip & family_masks[(f * F_step) + G_offset]) == 0)) {
								auto& tran_Hx = block_states[i + j].tran_Hx[f];
								decodeTran.template operator()<SymbolBackwardTranslator::tran_H_codeparam>(tran_Hx, (i >> 4));
							}
						}
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							// TODO: seems like bplane_skip check below is redundant 
							// already because handled properly when computing tran_G
							if ((tran_G[f].length == 0) & ((bplane_skip & family_masks[(f * F_step) + G_offset]) == 0)) {
								auto& tran_Hx = block_states[i + j].tran_Hx[f];
								for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
									if (tran_Hx[k].length == 0) {
										uint64_t extract_mask = family_masks[(f * F_step) + Hx_offset + k] & (~bplane_mask[i + j]);
										decodeTypesSigns.template operator()<SymbolBackwardTranslator::types_H_codeparam>((i >> 4), j, extract_mask);
									}
								}
							}
						}
					}
				}
			}
		}

		// stage 4
		i = 0;
		for (ptrdiff_t j = 0; j < output.size; ++j) {
			if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_4) == 0) {
				uint64_t effective_mask = bplane_mask[j] & (~bplane_skip);
				uint64_t block_bplane = input.extract_masked(effective_mask);
				block_bplane = decompose_bword(block_bplane, effective_mask);
				// Bits missing in the input due to subband scaling are taken into account
				// as current_bplane is zeroed on every bplane index loop pass.
				current_bplane[j] |= block_bplane;
			}

			bplane_mask[j] |= block_states[j].bplane_mask_transit;
			block_states[j].bplane_mask_transit = 0;
		}
		
		{
			auto accbit = [](T& dst, uint64_t val) -> void {
				dst = (dst << 1) | (val & 0x01);
			};
			accumulate_bplane<uint64_t, T>(current_bplane.data(), output.data.data()->content, output.size * items_per_block, accbit);
		}

		--b;
	}

	{
		// TODO: maybe implement predicate as utility function in util.h
		auto signset = [](T& dst, uint64_t predicate) -> void {
			int64_t spredicate = (int64_t)predicate;
			dst = (dst ^ (-spredicate)) + spredicate;
		};
		accumulate_bplane<uint64_t, T>(block_signs.data(), output.data.data()->content, output.size * items_per_block, signset);
	}

	// trick segment decoder to handle DC subband as unscaled already
	output.q = relu(((ptrdiff_t)(output.q)) - output.bit_shifts[0]);
	output.bdepthDc = relu(((ptrdiff_t)(output.bdepthDc)) - output.bit_shifts[0]);
	output.bit_shifts[0] = 0;
}

template <typename T, size_t alignment>
void BitPlaneXcoder<T, alignment>::set_use_heuristic_DC(bool value) {
	this->use_heuristic_DC = value;
}

template <typename T, size_t alignment>
bool BitPlaneXcoder<T, alignment>::get_use_heuristic_DC() const {
	return this->use_heuristic_DC;
}

template <typename T, size_t alignment>
void BitPlaneXcoder<T, alignment>::set_use_heuristic_bdepthAc(bool value) {
	this->use_heuristic_bdepthAc = value;
}

template <typename T, size_t alignment>
bool BitPlaneXcoder<T, alignment>::get_use_heuristic_bdepthAc() const {
	return this->use_heuristic_bdepthAc;
}
