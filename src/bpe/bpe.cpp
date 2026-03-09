#include "bpe.tpp"

#include <vector>
#include <deque>
#include <bit>
#include <numeric>
#include <type_traits>

#include "utility.hpp"

#include "aligned_vector.tpp"

#include "bitplane.tpp"
#include "symbolbuffer.hpp"
#include "symbol_translate.hpp"
#include "entropy_translate.hpp"

#include "constant.hpp"


// implementation section
// 
// BitPlaneEncoder implementation
template <typename T, size_t alignment>
template <typename obwT>
void BitPlaneEncoder<T, alignment>::encode(segment<T>& input, obitwrapper<obwT>& output) {
	if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_validation)) {
		// simple validation staff
		bool validation_error = false;
		if (input.bdepthDc < 1) {
			validation_error |= true;
		}
		if (input.size < 1) {
			validation_error |= true;
		}

		for (auto subband_shift : input.bit_shifts) {
			if (subband_shift > constants::subband::max_scale_shift) {
				validation_error |= true;
				break;
			}
		}

		if (input.plainDc.size() < input.size) {
			validation_error |= true;
		}
		if (input.quantizedDc.size() < input.size) {
			validation_error |= true;
		}
		if (input.quantizedBdepthAc.size() < input.size) {
			validation_error |= true;
		}
		if (input.data.size() < input.size) {
			validation_error |= true;
		}

		// TODO: need proper design to expose offset requirements to 
		// the code that sets up the buffers (i.e. segment_assembly)
		// 
		// checks against requirement of kOptimal
		if (input.quantizedDc.offset() < items_per_gaggle * 2) {
			validation_error |= true;
		}
		if (input.quantizedBdepthAc.offset() < items_per_gaggle * 2) {
			validation_error |= true;
		}

		if (validation_error) {
			// TODO: handle validation error, throw exception
			throw "validation failed.";
		}
	}

	if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_quant_DC)) {
	// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_quant_DC) == 0) {
		kParams quantizedDcParams = {
			input.quantizedDc.data(),
			input.size - 1,	// -1 because of a reference sample that is not part of the vector
			input.bdepthDc - input.q, 
			input.referenceSample
		}; 
		kEncode(quantizedDcParams, this->use_heuristic_DC, output);
	}

	if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_additional_DC)) {
	// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_additional_DC) == 0) {
		size_t bit_shift_LL3 = input.bit_shifts[0];
		size_t additional_bdepth = std::max(input.bdepthAc, bit_shift_LL3);
		if (input.q > additional_bdepth) {
			bplaneEncode(input.plainDc.data(), input.size, input.q, input.q - additional_bdepth, output);
		}
	}
	
	if (input.bdepthAc > 0) {
		if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_bdepth_AC)) {
		// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_bdepth_AC) == 0) {
			kParams quantizedBdepthAcParams = {
				input.quantizedBdepthAc.data(),
				input.size - 1,	// -1 because of a reference sample that is not part of the vector
				std::bit_width(input.bdepthAc),
				input.referenceBdepthAc
			};
			kEncode(quantizedBdepthAcParams, this->use_heuristic_bdepthAc, output);
		}
		encodeBpeStages(input, output);
	} // else no action needed, all DC data is encoded during addition DC encoding stage

	// it's caller responsibility to flush obitwrapper content after bpe encode
	// process is finished. That will produce necessary padding bits as described 
	// in 4.2.3.2.5. The caller may choose to fill the output stream with padding
	// bits up to SegByteLimit.
	// 
	// TODO: in header encoding module:
	// put obitwrapper buffer type parameter size as Header part 4 field
	// CodeWordLength, see 4.2.5.2.8
	// See 4.2.3.2.1, 4.2.3.2.5 and 4.2.5.2.8
}

template <typename T, size_t alignment>
template <typename D, typename obwT>
void BitPlaneEncoder<T, alignment>::kOptimal(kParams<D> params, obitwrapper<obwT>& output_stream) {
	// Requirements:
	// requires params.data's size to be extended to the nearest upper multiple 
	// of (items_per_gaggle * gaggles_per_iter) [= 32] at least;
	// requires params.bdepth to be greater than 0 (at least 1).

	size_t N = params.bdepth; // N is expected to be in range [2, 10]
	// N is a property of a whole segment. Therefore k_bound value is 
	// the same for every gaggle in the segment
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

	// zero placehoder for DC reference sample. 
	// zero remaining placeholders for dc elements at the end of the segmnet to extend 
	// the last gaggle to len=16 (but due to reference sample index -1 the last index 
	// is one less also)
	{
		params.data[-1] = 0;
		ptrdiff_t dc_boundary = params.datalength | gaggle_size_mask;
		// remember that reference sample is not counted in params.datalength
		for (ptrdiff_t i = params.datalength; i < dc_boundary; ++i) {
			params.data[i] = 0;
		}
		// and do not zero the rest part of buffer tail required by gaggles_per_iter
	}

	constexpr size_t k_simd_width = 8;
	constexpr size_t gaggles_per_iter = 2;
	std::vector<std::pair<size_t, ptrdiff_t>> k_options((params.datalength >> gaggle_size_shift) + 
		(gaggles_per_iter * 2)); // extended just in case

	uint_least16_t simdr_k_bound_mask[k_simd_width] = { 0 };
	for (ptrdiff_t i = k_bound; i < k_simd_width; ++i) {
		// fill mask values with maximum positive integer to effectuvely 
		// disable irrelevant k options for a given N parameter later
		// 
		// clear sign bit to make comparison operate correctly later
		simdr_k_bound_mask[i] = ((uint_least16_t)((ptrdiff_t)(-1))) >> 1;
	}

	constexpr uint_least16_t simdr_shift[k_simd_width] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	// constexpr std::array<uint_least16_t, k_simd_width> simdr_shift;
	// std::iota(simdr_shift.begin(), simdr_shift.end(), 1);
	
	// the loop below starts from reference sample at index -1, therefore 
	// 'le' is used to cover the very last item in params.data also
	for (ptrdiff_t i = 0; i <= params.datalength; i += items_per_gaggle * gaggles_per_iter) {
		uint_least16_t simdr_acc[gaggles_per_iter][k_simd_width] = { 0 };
		uint_fast16_t gpr_acc[gaggles_per_iter] = { 0 };

		// attempt to increase pipline load uniformity; repeat uniform operation blocks several times
		// should be beneficial for branch prediction in the second block as well
		for (ptrdiff_t si = 0; si < gaggles_per_iter; ++si) {
			for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
				uint_fast16_t gpr = params.data[i + (si * items_per_gaggle) + j - 1]; // -1 here because of reference sample
				gpr_acc[si] += gpr;
				uint_least16_t simdr[k_simd_width] = { 0 };
				for (ptrdiff_t vi = 0; vi < k_simd_width; ++vi) {
					simdr[vi] = gpr;
					simdr[vi] >>= simdr_shift[vi];
					simdr_acc[si][vi] += simdr[vi];
				}
			}
			for (ptrdiff_t vi = 0; vi < k_simd_width; ++vi) {
				// make results for k options exceeding k option number limit
				// too large to become the best option (effectively disable)
				//
				// this assigns the largest number that can be held by the type
				simdr_acc[si][vi] |= simdr_k_bound_mask[vi];
			}
		}

		ptrdiff_t k_option_index = i >> gaggle_size_shift;
		for (ptrdiff_t si = 0; si < gaggles_per_iter; ++si) {
			// There is no straitforward way to compare with uncoded option reliably due to different 
			// uncoded stream length for the first gaggle, subsequent gaggles and the last one. 
			// Encoded stream lengths can be compared safely on the other hand as the diff value due to
			// additional '1' bit is the same for all k options for a given gaggle.
			decltype(k_options)::value_type current_k = { gpr_acc[si], 0 };
			// hopefully this can be vectorized or at least well-predicted
			for (ptrdiff_t vi = 0; vi < k_simd_width; ++vi) {
				if (simdr_acc[si][vi] < current_k.first) {
					current_k.first = simdr_acc[si][vi];
					current_k.second = simdr_shift[vi];
				}
			}
			k_options[k_option_index + si] = current_k;
		}
	}

	// adjust parameters for the first gaggle (see 4.3.2.8 and 4.3.2.10, figures 4-8 and 4-9)
	{
		size_t k_bitsize = std::bit_width(N - 1u);
		// take into account '1' bits inserted into the stream after the first part of the codewords
		// to make it comparable with encoded stream lengths
		size_t k_uncoded_len = N * items_per_gaggle - items_per_gaggle;
		size_t k_uncoded_len_first = k_uncoded_len + 1 - N; // TODO: ensure that unsigned arithmetic is not reordered

		ptrdiff_t gaggle_index = 0;
		if (k_options[gaggle_index].first >= k_uncoded_len_first) {
			k_options[gaggle_index].second = k_uncoded_codeoption;
		}
		output_stream << vlw_t{ k_bitsize, (size_t)(k_options[gaggle_index].second) };
		output_stream << vlw_t{ N, (size_t)(params.reference) };

		ptrdiff_t i;
		bool execute_preamble = false;
		for (i = -1; i < ((ptrdiff_t)(params.datalength - items_per_gaggle)); i += items_per_gaggle) {
			if (execute_preamble) {
				gaggle_index = (i >> gaggle_size_shift) + 1;
				if (k_options[gaggle_index].first >= k_uncoded_len) {
					k_options[gaggle_index].second = k_uncoded_codeoption;
				}
				output_stream << vlw_t{ k_bitsize, (size_t)(k_options[gaggle_index].second) };
			}
			execute_preamble = true;

			if (k_options[gaggle_index].second != k_uncoded_codeoption) {
				for (ptrdiff_t j = (i < 0); j < items_per_gaggle; ++j) {
					output_stream << vlw_t{ (size_t)((params.data[i + j] >> k_options[gaggle_index].second) + 1), 1 };
				}
			} else {
				k_options[gaggle_index].second = N;
			}

			for (ptrdiff_t j = (i < 0); j < items_per_gaggle; ++j) {
				output_stream << vlw_t{ (size_t)(k_options[gaggle_index].second), (size_t)(params.data[i + j]) };
			}
		}

		ptrdiff_t last_gaggle_len = params.datalength - i;
		size_t k_uncoded_len_last = N * last_gaggle_len - last_gaggle_len;
		if (execute_preamble) {
			gaggle_index = (i >> gaggle_size_shift) + 1;
			if (k_options[gaggle_index].first >= k_uncoded_len_last) {
				k_options[gaggle_index].second = k_uncoded_codeoption;
			}
			output_stream << vlw_t{ k_bitsize, (size_t)(k_options[gaggle_index].second) };
		}
		execute_preamble = true;

		if (k_options[gaggle_index].second != k_uncoded_codeoption) {
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
void BitPlaneEncoder<T, alignment>::kHeuristic(kParams<D> params, obitwrapper<obwT>& output_stream) {
	size_t N = params.bdepth; // N is expected to be in range [2, 10]
	size_t k_bitsize = std::bit_width(N - 1u);
	ptrdiff_t J = items_per_gaggle - 1; // =15, first gaggle
	for (ptrdiff_t i = 0; i < params.datalength; i += J) {
		if (i > 0) {
			J = std::min(items_per_gaggle, params.datalength - i);
		}
		size_t delta = 0;
		for (ptrdiff_t j = 0; j < J; ++j) {
			delta += params.data[i + j];
		}

		std::array<size_t, 3> predicates_set_1 = {
			delta * 64, 
			J * 207, 
			J * (1 << (N + 5)), 
		};
		std::array<size_t, 3> predicates_set_2 = {
			J * 23 * (1 << N), 
			delta * 128, 
			(delta * 128) + (J * 49), 
		};

		ptrdiff_t k = k_uncoded_codeoption;
		if (predicates_set_1[0] >= predicates_set_2[0]) {
			k = k_uncoded_codeoption;
		} else if (predicates_set_1[1] > predicates_set_2[1]) {
			k = 0;
		} else if (predicates_set_1[2] <= predicates_set_2[2]) {
			k = N - 2;
		} else {
			// TODO: check underflow/arithmetic/input values, potential UB
			k = std::bit_width(predicates_set_2[2] / J) - 7;
		}

		output_stream << vlw_t{ k_bitsize, (size_t)(k) };
		if (i == 0) {
			output_stream << vlw_t{ N, (size_t)(params.reference) };
		}
		if (k != k_uncoded_codeoption) {
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
void BitPlaneEncoder<T, alignment>::kEncode(kParams<D> params, bool use_heuristic, obitwrapper<obwT>& output_stream) {
	// Requirements:
	// requires params.data's size to be extended to the nearest upper multiple 
	// of (items_per_gaggle * gaggles_per_iter) [= 32] at least [by kOptimal]
	
	// The adjustment below is necessary. 
	// 
	// params.bdepthis is guaranteed to be at least 1 for quantizedBdepthAc per 
	// requirement that bdepthAC is greater than 0 when invoking this function; 
	// 
	// (bdepthDc - q`) is guaranteed to be at least 1 per q` computation 
	// approach for quantizedDc;
	// 
	// but per 4.3.1.3: q = max(q`, BitShift(LL3)).
	// In a case when the whole DC subband is all zeroes, bdepthDc is 
	// necessarily 1 (because DC coeffs are encoded as signed integers in 
	// 2-complement form), and q` is guaranteed to be equal to 0. 
	// But BitShift(LL3) (aka bit_shifts[0]) may have arbitrary value in range 
	// [0-3] (scaling zeroes results in zeroes, so bdepthDc is still 1 and the 
	// value is still valid), that causes q to possibly have values greater 
	// than 0. As a result, (input.bdepthDc - input.q) may be negative, and max 
	// call below is indeed necessary.
	//
	// see 4.3.2.1
	params.bdepth = std::max(params.bdepth, (decltype(params.bdepth))(min_N_value));
	if (params.bdepth > 1) {
		if (use_heuristic) {
			kHeuristic(params, output_stream);
		} else {
			kOptimal(params, output_stream);
		}
	} else {
		size_t N = params.bdepth;	// N == 1
		output_stream << vlw_t{ N, (size_t)(params.reference) };
		for (ptrdiff_t i = 0; i < params.datalength; ++i) {
			output_stream << vlw_t{ N, (size_t)(params.data[i]) };
		}
	}
}

template <typename T, size_t alignment>
template <typename obwT>
void BitPlaneEncoder<T, alignment>::encodeBpeStages(segment<T>& input, obitwrapper<obwT>& output) {
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

	constexpr size_t sign_bindex = (sizeof(T) << 3) - 1;
	T* raw_block_data = std::assume_aligned<(alignment * sizeof(T))>(input.data.data()->content); // TODO: check and refactor
	size_t raw_block_data_size = input.size * items_per_block;

	bplane_static<sign_bindex>(raw_block_data, raw_block_data_size, block_signs_bitwrapper, 
		[](T& item) -> void { item = magnitude(item); });

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
		std::array<ptrdiff_t, vlw_lengths_count> entropy_codeoptions = {
			entropy_uncoded_codeoption, 
			entropy_uncoded_codeoption, 
			entropy_uncoded_codeoption, 
			entropy_uncoded_codeoption, 
			entropy_uncoded_codeoption 
		}; // { -1, -1, -1, -1, -1 };
		std::array<bool, vlw_lengths_count> code_marker_written = { true, true, false, false, false };
	};

	std::vector<block_bpe_meta> block_states(input.size);
	std::vector<gaggle_bpe_meta> gaggle_states((input.size + items_per_gaggle) >> gaggle_size_shift);

	auto compute_gaggle_wlv = [&] <bool enable_short_circuit = false> (ptrdiff_t i, size_t gaggle_len) -> void {
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

		// compute vlw for every block in the current gaggle
		for (ptrdiff_t j = 0; j < gaggle_len; ++j) {
			std::array<typename decltype(family_masks)::value_type, family_masks.size()> prepared_block = { 0 };
			for (ptrdiff_t k = 0; k < family_masks.size(); ++k) {
				prepared_block[k] = current_bplane[i + j] & family_masks[k];
			}

			auto accumulate_tran_vlw = [&](dense_vlw_t& vlw, size_t index, bool skip = false) -> bool {
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

			auto compute_plain_vlw = [&](dense_vlw_t& typesVlw, dense_vlw_t& signsVlw, size_t index, bool skip = false) {
				// All active elements that are not of type 2 (or -1) yet, mask is applicable
				// to get values '0' and '1' of dedicated bits
				uint64_t effective_mask = (family_masks[index] & (~bplane_mask[i + j])) & (-((int64_t)(!skip)));
				if constexpr (enable_short_circuit) {
					effective_mask &= ~bplane_skip;
				}

				typesVlw = combine_bword(prepared_block[index], effective_mask);
				// May relief contention for a single item in current_bplane[i + j] and use 
				// masked copy instead, that should be just used by computing transition word
				signsVlw = combine_bword(block_signs[i + j], (prepared_block[index] & effective_mask));

				block_states[i + j].bplane_mask_transit |= (prepared_block[index] & effective_mask);
			};

			// set tran words to completely clean states to mitigate interfering with leftovers
			// of the previous bitplane. Also allows write operations only with no need to read
			// the old state and elide assosiated memory ops dependencies.
			block_states[i + j].tran_B = dense_vlw_t{ 0, 0 };
			bool skip_descendants = accumulate_tran_vlw(block_states[i + j].tran_B, B_index);
			// subband scaling for P coeffs is handled in compute_plain_vlw
			compute_plain_vlw(block_states[i + j].types_P, block_states[i + j].signs_P, P_index);
			symbol_fwd_translator.translate(block_states[i + j].types_P);
			entropy_buffers.append_vlw(block_states[i + j].types_P);

			if (!skip_descendants) {
				block_states[i + j].tran_D = dense_vlw_t{ 0, 0 };
				block_states[i + j].tran_G = dense_vlw_t{ 0, 0 };
				for (ptrdiff_t f = 0; f < F_num; ++f) {
					// (skip = false) is implied because the case is covered by control flow dependency
					// introduced by branch few lines above
					bool skip_tran_Gx = accumulate_tran_vlw(block_states[i + j].tran_D, (f * F_step) + D_offset);
					bool skip_tran_Hx = accumulate_tran_vlw(block_states[i + j].tran_G, (f * F_step) + G_offset, skip_tran_Gx);

					// check bplane_skip here for child coeffs
					bool skip_children = false;
					if constexpr (enable_short_circuit) {
						skip_children = !((bplane_skip & family_masks[(f * F_step) + C_offset]) == 0);
					}
					if (!skip_children) {
						compute_plain_vlw(block_states[i + j].types_C[f], block_states[i + j].signs_C[f],
							(f * F_step) + C_offset, skip_tran_Gx);
						symbol_fwd_translator.translate(block_states[i + j].types_C[f]);
						entropy_buffers.append_vlw(block_states[i + j].types_C[f]);
					} else {
						block_states[i + j].types_C[f] = dense_vlw_t{ 0, 0 };
						block_states[i + j].signs_C[f] = dense_vlw_t{ 0, 0 };
					}

					block_states[i + j].tran_H[f] = dense_vlw_t{ 0, 0 };
					// check bplane_skip here for grandchildren coeffs
					bool skip_grandchildren = false;
					if constexpr (enable_short_circuit) {
						skip_grandchildren = !((bplane_skip & family_masks[(f * F_step) + G_offset]) == 0);
					}
					if (!skip_grandchildren) {
						for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
							bool skip_Hx = accumulate_tran_vlw(block_states[i + j].tran_H[f], 
								(f * F_step) + Hx_offset + k, skip_tran_Hx);
							compute_plain_vlw(block_states[i + j].types_H[f][k], block_states[i + j].signs_H[f][k],
								(f * F_step) + Hx_offset + k, skip_Hx);
							symbol_fwd_translator.translate<symbol_types_H_codeoption>(block_states[i + j].types_H[f][k]);
							entropy_buffers.append_vlw(block_states[i + j].types_H[f][k]);
						}
					} else {
						for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
							block_states[i + j].types_H[f][k] = dense_vlw_t{ 0, 0 };
							block_states[i + j].signs_H[f][k] = dense_vlw_t{ 0, 0 };
						}
					}
				}
			}

			symbol_fwd_translator.translate<symbol_tran_D_codeoption>(block_states[i + j].tran_D);
			entropy_buffers.append_vlw(block_states[i + j].tran_D);
			symbol_fwd_translator.translate(block_states[i + j].tran_G);
			entropy_buffers.append_vlw(block_states[i + j].tran_G);
			for (size_t k = 0; k < block_states[i + j].tran_H.size(); ++k) {
				symbol_fwd_translator.translate<symbol_tran_H_codeoption>(block_states[i + j].tran_H[k]);
				entropy_buffers.append_vlw(block_states[i + j].tran_H[k]);
			}
		}

		// compute gaggle parameters based on block states
		ptrdiff_t gaggle_index = i >> gaggle_size_shift;
		std::array<std::pair<size_t, ptrdiff_t>, vlw_lengths_count> optimal_code_options = { {
			{ 0, entropy_uncoded_codeoption }, // fictive item for null-words
			{ 0, entropy_uncoded_codeoption }, // fictive item for one-bit words
			{ entropy_buffers[2].bitlength(), entropy_uncoded_codeoption },
			{ entropy_buffers[3].bitlength(), entropy_uncoded_codeoption },
			{ entropy_buffers[4].bitlength(), entropy_uncoded_codeoption }
		} };

		auto update_optimal_code_option = [&](size_t vlw_len, size_t option) -> void {
			size_t current_bitsize = 0;
			while (entropy_buffers[vlw_len]) {
				current_bitsize += entropy_translator.word_length(entropy_buffers[vlw_len].next(), vlw_len, option);
			}
			if (current_bitsize < optimal_code_options[vlw_len].first) {
				optimal_code_options[vlw_len] = { current_bitsize, option };
			}
			entropy_buffers[vlw_len].restart();
		};

		update_optimal_code_option(2, 0);
		update_optimal_code_option(3, 0);
		update_optimal_code_option(3, 1);
		update_optimal_code_option(4, 0);
		update_optimal_code_option(4, 1);
		update_optimal_code_option(4, 2);

		for (ptrdiff_t k = min_coded_vlw_length; k <= max_coded_vlw_length; ++k) {
			gaggle_states[gaggle_index].entropy_codeoptions[k] = optimal_code_options[k].second;
			gaggle_states[gaggle_index].code_marker_written[k] = false;
			entropy_buffers[k].reset();
		}
	};
	
	auto encode_vlw = [&](dense_vlw_t& vlw, ptrdiff_t gaggle_index) -> void {
		size_t vlw_length = vlw.length;
		ptrdiff_t codeoption = gaggle_states[gaggle_index].entropy_codeoptions[vlw_length];
		if (!gaggle_states[gaggle_index].code_marker_written[vlw_length]) {
			// for word length 0 and 1 predicate is always false, should
			// never be executed for those lengths
			gaggle_states[gaggle_index].code_marker_written[vlw_length] = true;
			size_t codeoption_bitsize = std::bit_width(vlw_length - 1u); // TODO: C++23 there should be size_t suffix instead of u
			output << vlw_t{ codeoption_bitsize, (vlw_t::type)(codeoption) };
		}
		output << entropy_translator.translate(vlw, codeoption);
	};

	size_t input_size_truncated = input.size & (~gaggle_size_mask);
	size_t last_gaggle_size = input.size & gaggle_size_mask;

	size_t b = input.bdepthAc - 1; // requires bdepthAc > 0; check at caller side
	for (; b >= constants::subband::max_scale_shift; --b) {
		if (current_bplane.empty()) {
			// Per the loop conditions b is guaranteed to be more than or equal to 3, 
			// that means at least 4 bitplanes are available always. 
			size_t bplane_num_to_extract = 4;
			// fills current_bplane container
			bplaneEncode(raw_block_data, raw_block_data_size, b, bplane_num_to_extract, current_bitplane_bitwrapper);
		}

		// stage 0
		if (b < input.q) {
			// q may be less than bdepthAc, e.g.:
			// bdepthAc = 3; bdepthDc = 10; bitShift(LL3) = 0
			// => 
			// q = 2
			//
			if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_0)) {
			// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_0) == 0) {
				bplaneEncode(input.plainDc.data(), input.size, b, 1, output); // the output buffer can still not be flushed yet
			}
		}

		// interleaving bpe stages
		{
			ptrdiff_t i = 0;
			for (; i < input_size_truncated; i += items_per_gaggle) {
				// TODO: PERF NOTE: here is concurrent execution optimization 
				// opportunity. Conceptually read different memory via same
				// references/pointers and writes to different memory addresses 
				// (but adjacent, may need to check cache-line layout).
				compute_gaggle_wlv(i, items_per_gaggle);
			}
			if (last_gaggle_size > 0) { // check needed because of computeGaggleParams
				compute_gaggle_wlv(i, last_gaggle_size);
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
			if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_1)) {
			// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_1) == 0) {
				i = 0;
				for (; i < input_size_truncated; i += items_per_gaggle) {
					for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
						encode_vlw(block_states[i + j].types_P, (i >> gaggle_size_shift));
						output << block_states[i + j].signs_P;
					}
				}
				for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
					encode_vlw(block_states[i + j].types_P, (i >> gaggle_size_shift));
					output << block_states[i + j].signs_P;
				}
			}
			
			// stage 2
			if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_2) || 
					dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_3)) {
			// if constexpr (((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) || 
			// 		((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_3) == 0)) {
				i = 0;
				for (; i < input_size_truncated; i += items_per_gaggle) {
					for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
						output << block_states[i + j].tran_B;
						encode_vlw(block_states[i + j].tran_D, (i >> gaggle_size_shift));
						if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_2)) {
						// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
							for (ptrdiff_t f = 0; f < F_num; ++f) {
								encode_vlw(block_states[i + j].types_C[f], (i >> gaggle_size_shift));
								output << block_states[i + j].signs_C[f];
							}
						}
					}
				}
				for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
					output << block_states[i + j].tran_B;
					encode_vlw(block_states[i + j].tran_D, (i >> gaggle_size_shift));
					if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_2)) {
					// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							encode_vlw(block_states[i + j].types_C[f], (i >> gaggle_size_shift));
							output << block_states[i + j].signs_C[f];
						}
					}
				}
			}

			// stage 3
			if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_3)) {
			// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_3) == 0) {
				i = 0;
				for (; i < input_size_truncated; i += items_per_gaggle) {
					for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
						encode_vlw(block_states[i + j].tran_G, (i >> gaggle_size_shift));
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							encode_vlw(block_states[i + j].tran_H[f], (i >> gaggle_size_shift));
						}
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
								encode_vlw(block_states[i + j].types_H[f][k], (i >> gaggle_size_shift));
								output << block_states[i + j].signs_H[f][k];
							}
						}
					}
				}
				for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
					encode_vlw(block_states[i + j].tran_G, (i >> gaggle_size_shift));
					for (ptrdiff_t f = 0; f < F_num; ++f) {
						encode_vlw(block_states[i + j].tran_H[f], (i >> gaggle_size_shift));
					}
					for (ptrdiff_t f = 0; f < F_num; ++f) {
						for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
							encode_vlw(block_states[i + j].types_H[f][k], (i >> gaggle_size_shift));
							output << block_states[i + j].signs_H[f][k];
						}
					}
				}
			}

			// stage 4
			i = 0; // not really necessary, just for debugging consistency
			for (ptrdiff_t j = 0; j < input.size; ++j) {
				if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_4)) {
				// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_4) == 0) {
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
			// PERF NOTE: will 'gt' be predicted better than 'eq'? Just curious
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
			bplaneEncode(raw_block_data, raw_block_data_size, b, bplane_num_to_extract, current_bitplane_bitwrapper);
		}

		// stage 0
		// 
		// As per 4.3.1.2 and 4.3.1.3, q and BitShift LL3 can evaluate to mutually independent values
		// with respect to the lower bound, therefore both checks should be performed; no skip is 
		// possible due to a priori knowledge.
		// 
		if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_0)) {
		// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_0) == 0) {
			if (((bplane_skip & family_masks[DC_index]) == 0) & (b < input.q)) {
				bplaneEncode(input.plainDc.data(), input.size, b, 1, output);
			}
		}

		ptrdiff_t i = 0;
		for (; i < input_size_truncated; i += items_per_gaggle) {
			compute_gaggle_wlv.template operator()<true>(i, items_per_gaggle);
		}
		if (last_gaggle_size > 0) {
			compute_gaggle_wlv.template operator()<true>(i, last_gaggle_size);
		}

		// stage 1
		// 
		// Check for complex bplane_skip field consisting of 3 independent single bit 
		// masks for parent coeffs, therefore check for unequality instead of 'eq' 0.
		//
		if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_1)) {
		// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_1) == 0) {
			i = 0;
			if ((bplane_skip & family_masks[P_index]) != family_masks[P_index]) {
				for (; i < input_size_truncated; i += items_per_gaggle) {
					for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
						encode_vlw(block_states[i + j].types_P, (i >> gaggle_size_shift));
						output << block_states[i + j].signs_P;
					}
				}
				for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
					encode_vlw(block_states[i + j].types_P, (i >> gaggle_size_shift));
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
		if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_2) || 
				dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_3)) {
		// if constexpr (((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) || 
		// 		((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_3) == 0)) {
			i = 0;
			for (; i < input_size_truncated; i += items_per_gaggle) {
				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
					output << block_states[i + j].tran_B;
					encode_vlw(block_states[i + j].tran_D, (i >> gaggle_size_shift));
					if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_2)) {
					// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							encode_vlw(block_states[i + j].types_C[f], (i >> gaggle_size_shift));
							output << block_states[i + j].signs_C[f];
						}
					}
				}
			}
			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
				output << block_states[i + j].tran_B;
				encode_vlw(block_states[i + j].tran_D, (i >> gaggle_size_shift));
				if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_2)) {
				// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
					for (ptrdiff_t f = 0; f < F_num; ++f) {
						encode_vlw(block_states[i + j].types_C[f], (i >> gaggle_size_shift));
						output << block_states[i + j].signs_C[f];
					}
				}
			}
		}

		// stage 3
		if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_3)) {
		// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_3) == 0) {
			i = 0;
			for (; i < input_size_truncated; i += items_per_gaggle) {
				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
					// (bplane_skip & family_masks[B_index]) is unlikely to equal to zero, 
					// therefore such a check is not performed. All dependencies are taken into
					// account and all the related words have length 0 per implementation.
					//

					encode_vlw(block_states[i + j].tran_G, (i >> gaggle_size_shift));
					for (ptrdiff_t f = 0; f < F_num; ++f) {
						encode_vlw(block_states[i + j].tran_H[f], (i >> gaggle_size_shift));
					}
					for (ptrdiff_t f = 0; f < F_num; ++f) {
						if ((bplane_skip & family_masks[(f * F_step) + G_offset]) == 0) {
							for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
								encode_vlw(block_states[i + j].types_H[f][k], (i >> gaggle_size_shift));
								output << block_states[i + j].signs_H[f][k];
							}
						}
					}
				}
			}
			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {
				encode_vlw(block_states[i + j].tran_G, (i >> gaggle_size_shift));
				for (ptrdiff_t f = 0; f < F_num; ++f) {
					encode_vlw(block_states[i + j].tran_H[f], (i >> gaggle_size_shift));
				}
				for (ptrdiff_t f = 0; f < F_num; ++f) {
					if ((bplane_skip & family_masks[(f * F_step) + G_offset]) == 0) {
						for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
							encode_vlw(block_states[i + j].types_H[f][k], (i >> gaggle_size_shift));
							output << block_states[i + j].signs_H[f][k];
						}
					}
				}
			}
		}

		// stage 4
		i = 0; // not really necessary, just for convinience during debugging
		for (ptrdiff_t j = 0; j < input.size; ++j) {
			if constexpr (dbg::bpe::encoder::if_enabled(dbg::bpe::mask_stage_4)) {
			// if constexpr ((dbg::bpe::encoder::disabled_stages & dbg::bpe::mask_stage_4) == 0) {
				output << combine_bword(current_bplane[j], (bplane_mask[j] & (~bplane_skip)));
			}
			bplane_mask[j] |= block_states[j].bplane_mask_transit;
			block_states[j].bplane_mask_transit = 0;
		}

		current_bplane.erase(current_bplane.begin(), current_bplane.begin() + input.size);
		--b;	// formally UB when b == 0
	} while (next_bplane);
}

template <typename T, size_t alignment>
void BitPlaneEncoder<T, alignment>::set_use_heuristic_DC(bool value) {
	this->use_heuristic_DC = value;
}

template <typename T, size_t alignment>
bool BitPlaneEncoder<T, alignment>::get_use_heuristic_DC() const {
	return this->use_heuristic_DC;
}

template <typename T, size_t alignment>
void BitPlaneEncoder<T, alignment>::set_use_heuristic_bdepthAc(bool value) {
	this->use_heuristic_bdepthAc = value;
}

template <typename T, size_t alignment>
bool BitPlaneEncoder<T, alignment>::get_use_heuristic_bdepthAc() const {
	return this->use_heuristic_bdepthAc;
}


template <typename T, size_t alignment>
void BitPlaneEncoder<T, alignment>::set_stop_after_DC(bool value) {
	// TODO: implement logic
}

template <typename T, size_t alignment>
void BitPlaneEncoder<T, alignment>::set_stop_at_bplane(size_t bplane_index, size_t stage_index /* = 0b11 == stage_4 */) {
	// TODO: implement logic
}


// BitPlaneDecoder implementation
template <typename T>
template <typename ibwT>
void BitPlaneDecoder<T>::decode(segment<T>& output, ibitwrapper<ibwT>& input) {
	// segment initialization
	// TODO: check initialization logic
	output.data = decltype(output.data)(output.size);
	output.referenceSample = 0;
	output.quantizedDc = aligned_vector<T>(output.size);
	output.plainDc = aligned_vector<T>(output.size);
	output.plainDc.assign(0);
	output.referenceBdepthAc = 0;
	output.quantizedBdepthAc = aligned_vector<size_t>(output.size);

	output.q = quant_dc(output.bdepthDc, output.bdepthAc, output.bit_shifts[0]);

	if constexpr (dbg::bpe::decoder::if_enabled(dbg::bpe::mask_quant_DC)) {
	// if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_quant_DC) == 0) {
		kParams quantizedDcParams = {
			output.quantizedDc.data(),
			output.size - 1,	// -1 because of a reference sample that is not part of the vector
			output.bdepthDc - output.q,
			(typename decltype(output.quantizedDc)::type)(0)
		};
		kDecode(quantizedDcParams, input);
		output.referenceSample = quantizedDcParams.reference;
	}

	if constexpr (dbg::bpe::decoder::if_enabled(dbg::bpe::mask_additional_DC)) {
	// if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_additional_DC) == 0) {
		size_t bit_shift_LL3 = output.bit_shifts[0];
		size_t additional_bdepth = std::max(output.bdepthAc, bit_shift_LL3);
		if (output.q > additional_bdepth) {
			bplaneDecode(output.plainDc.data(), output.size, output.q - additional_bdepth, input);
		}
	}

	if (output.bdepthAc > 0) {
		if constexpr (dbg::bpe::decoder::if_enabled(dbg::bpe::mask_bdepth_AC)) {
		// if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_bdepth_AC) == 0) {
			kParams quantizedBdepthAcParams = {
				output.quantizedBdepthAc.data(),
				output.size - 1,	// -1 because of a reference sample that is not part of the vector
				std::bit_width(output.bdepthAc),
				(typename decltype(output.quantizedBdepthAc)::type)(0)
			};
			kDecode(quantizedBdepthAcParams, input);
			// TODO: maybe there's a bug, check usage and actual values during encoding and decoding.
			// Passed all current tests without assignment of the result (but shouldn't have).
			// The line below was absent.
			output.referenceBdepthAc = quantizedBdepthAcParams.reference;
		}
		decodeBpeStages(output, input);
	}
}

template <typename T>
template <typename D, typename ibwT>
void BitPlaneDecoder<T>::kReverse(kParams<D>& params, ibitwrapper<ibwT>& input_stream) {
	size_t N = params.bdepth; // N is expected to be in range [2, 10]

	// adjust parameters for the first gaggle (see 4.3.2.8 and 4.3.2.10, figures 4-8 and 4-9)
	size_t k_bitsize = std::bit_width(N - 1u);
	size_t k_uncoded_opt = ~(k_uncoded_codeoption << k_bitsize);
	size_t current_k_opt = input_stream.extract(k_bitsize);
	params.reference = signext(input_stream.extract(N), N);

	ptrdiff_t i;
	bool current_uncoded = false;
	bool execute_preamble = false;
	for (i = -1; i < ((ptrdiff_t)(params.datalength - items_per_gaggle)); i += items_per_gaggle) {
		if (execute_preamble) {
			current_k_opt = input_stream.extract(k_bitsize);
		}
		execute_preamble = true;

		current_uncoded = (current_k_opt == k_uncoded_opt);
		if (!current_uncoded) {
			for (ptrdiff_t j = (i < 0); j < items_per_gaggle; ++j) {
				params.data[i + j] = input_stream.extract_next_one();
			}
		} else {
			current_k_opt = N;
		}

		for (ptrdiff_t j = (i < 0); j < items_per_gaggle; ++j) {
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

template <typename T>
template <typename D, typename ibwT>
void BitPlaneDecoder<T>::kDecode(kParams<D>& params, ibitwrapper<ibwT>& input_stream) {
	// See notes for BitPlaneEncoder::kEncode.
	// see 4.3.2.1
	params.bdepth = std::max(params.bdepth, (decltype(params.bdepth))(min_N_value));
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

template <typename T>
template <typename ibwT>
void BitPlaneDecoder<T>::decodeBpeStages(segment<T>& output, ibitwrapper<ibwT>& input) {
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
		std::array<ptrdiff_t, vlw_lengths_count> entropy_codeoptions = { 
			// first two are important and assigned -1
			entropy_uncoded_codeoption, 
			entropy_uncoded_codeoption, 
			// the rest can be assigned any value because will be rewritten 
			// by actual value extracted from input bit stream
			entropy_uncoded_codeoption,	
			entropy_uncoded_codeoption,	
			entropy_uncoded_codeoption 
		}; // { -1, -1, -1, -1, -1 };
		std::array<bool, vlw_lengths_count> code_marker_processed = { true, true, false, false, false };
	};

	std::vector<block_bpe_meta> block_states(output.size);
	std::vector<gaggle_bpe_meta> gaggle_states((output.size + items_per_gaggle) >> gaggle_size_shift);

	auto decode_vlw = [&](size_t vlw_length, size_t gaggle_index) -> vlw_t {
		ptrdiff_t codeoption = gaggle_states[gaggle_index].entropy_codeoptions[vlw_length];
		if (gaggle_states[gaggle_index].code_marker_processed[vlw_length] == false) {
			gaggle_states[gaggle_index].code_marker_processed[vlw_length] = true;
			size_t codeoption_bitsize = std::bit_width(vlw_length - 1u);
			ptrdiff_t uncoded_option = ~(((ptrdiff_t)(entropy_uncoded_codeoption)) << codeoption_bitsize);
			codeoption = input.extract(codeoption_bitsize);
			if (codeoption == uncoded_option) {
				codeoption = signext(codeoption, codeoption_bitsize);
			}
			gaggle_states[gaggle_index].entropy_codeoptions[vlw_length] = codeoption;
		}
		return entropy_translator.extract(input, vlw_length, codeoption);
	};

	auto decode_plain_vlw = [&]<size_t codeoption = 0>(ptrdiff_t gaggle_index, ptrdiff_t block_index, uint64_t mask) -> void {
		ptrdiff_t i = gaggle_index << gaggle_size_shift;
		ptrdiff_t j = block_index;
		size_t bitsize_types = std::popcount(mask);
		vlw_t types = decode_vlw(bitsize_types, gaggle_index);
		symbol_bwd_translator.translate<codeoption>(types);
		size_t bitsize_signs = std::popcount(types.value);
		uint64_t signs = input.extract(bitsize_signs);
		uint64_t extracted_types = decompose_bword(types.value, mask);
		uint64_t extracted_signs = decompose_bword(signs, extracted_types);
		block_signs[i + j] |= extracted_signs;
		block_states[i + j].bplane_mask_transit |= extracted_types;
		current_bplane[i + j] |= extracted_types;
	};

	auto decode_tran_vlw = [&]<size_t codeoption = 0>(auto& tran, ptrdiff_t gaggle_index, size_t dependency_mask = 0x0ff) -> void {
		size_t tran_mask = 0;
		// The loop below sets implicit requirement on tran to be collection (provide begin() and end())
		// and have the contained type convertible to dense_vlw_t (that is, _vlw_t)
		for (dense_vlw_t item : tran) {
			tran_mask <<= 1;
			tran_mask |= item.length;
		}
		tran_mask &= dependency_mask;

		vlw_t tran_raw = decode_vlw(std::popcount(tran_mask), gaggle_index);
		symbol_bwd_translator.translate<codeoption>(tran_raw);

		for (ptrdiff_t k = tran.size() - 1; k >= 0; --k) {
			size_t effective_length = (tran_mask & 0x01);
			tran[k].value = (tran_raw.value & 0x01) & effective_length;
			tran_raw.value >>= effective_length;
			tran[k].length &= (~tran[k].value);
			tran_mask >>= 1;
		}
	};

	size_t last_gaggle_start = output.size & (~gaggle_size_mask);
	size_t last_gaggle_size = output.size & gaggle_size_mask;

	ptrdiff_t b = output.bdepthAc;
	// TODO: segment decoding should leave the segment data in a valid state when exception 
	// is thrown due to end of data/byte limit.
	// The segment data should be upscaled to it's target bitness, last partially decoded 
	// bitplane should be decomposed into corresponding coefficient values; all adjustment 
	// handlers should be executed on segment data.
	// 
	// It can be done via local handler types destructors.
	//
	while (b > 0) {
		for (ptrdiff_t i = 0; i < output.bit_shifts.size(); ++i) {
			if (output.bit_shifts[i] == b) {
				bplane_skip |= bitshift_masks[i];
			}
		}

		// stage 0
		if constexpr (dbg::bpe::decoder::if_enabled(dbg::bpe::mask_stage_0)) {
		// if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_0) == 0) {
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
			gaggle_states[i >> gaggle_size_shift].code_marker_processed = { true, true, false, false, false };
			if constexpr (dbg::bpe::decoder::if_enabled(dbg::bpe::mask_stage_1)) {
			// if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_1) == 0) {
				if ((bplane_skip & family_masks[P_index]) != family_masks[P_index]) {
					size_t current_gaggle_size = (i == last_gaggle_start) ? last_gaggle_size : items_per_gaggle;
					for (ptrdiff_t j = 0; j < current_gaggle_size; ++j) {
						uint64_t types_P_extract_mask = (family_masks[P_index] & (~bplane_mask[i + j])) & (~bplane_skip);
						decode_plain_vlw((i >> gaggle_size_shift), j, types_P_extract_mask);
					}
				}
			}
		}
		
		// stage 2
		if constexpr (dbg::bpe::decoder::if_enabled(dbg::bpe::mask_stage_2) || 
				dbg::bpe::decoder::if_enabled(dbg::bpe::mask_stage_3)) {
		// if constexpr (((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) || 
		// 		((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_3) == 0)) {
			i = 0;
			for (; i < output.size; i += items_per_gaggle) {
				size_t current_gaggle_size = (i == last_gaggle_start) ? last_gaggle_size : items_per_gaggle;
				for (ptrdiff_t j = 0; j < current_gaggle_size; ++j) {
					dense_vlw_t& tran_B = block_states[i + j].tran_B;
					tran_B.length &= ((bplane_skip & family_masks[B_index]) != family_masks[B_index]);
					tran_B.value = input.extract(tran_B.length);
					tran_B.length &= (~tran_B.value);
					if (tran_B.length == 0) {
						size_t tran_D_dependency_mask = 0;
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							tran_D_dependency_mask <<= 1;
							tran_D_dependency_mask |= 
								((bplane_skip & family_masks[(f * F_step) + D_offset]) != family_masks[(f * F_step) + D_offset]);
						}
						auto& tran_D = block_states[i + j].tran_D;
						decode_tran_vlw.template operator()<symbol_tran_D_codeoption>(tran_D, (i >> gaggle_size_shift), tran_D_dependency_mask);
						
						if constexpr (dbg::bpe::decoder::if_enabled(dbg::bpe::mask_stage_2)) {
						// if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_2) == 0) {
							for (ptrdiff_t f = 0; f < F_num; ++f) {
								if ((tran_D[f].length == 0) & ((bplane_skip & family_masks[(f * F_step) + C_offset]) == 0)) {
									uint64_t types_C_extract_mask = family_masks[(f * F_step) + C_offset] & (~bplane_mask[i + j]);
									decode_plain_vlw((i >> gaggle_size_shift), j, types_C_extract_mask);
								}
							}
						}
					}
				}
			}
		}

		// stage 3
		if constexpr (dbg::bpe::decoder::if_enabled(dbg::bpe::mask_stage_3)) {
		// if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_3) == 0) {
			i = 0;
			for (; i < output.size; i += items_per_gaggle) {
				size_t current_gaggle_size = (i == last_gaggle_start) ? last_gaggle_size : items_per_gaggle;
				for (ptrdiff_t j = 0; j < current_gaggle_size; ++j) {
					if (block_states[i + j].tran_B.length == 0) {
						auto& tran_D = block_states[i + j].tran_D;
						size_t tran_G_dependency_mask = 0;
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							tran_G_dependency_mask <<= 1;
							tran_G_dependency_mask |= 
								((bplane_skip & family_masks[(f * F_step) + G_offset]) != family_masks[(f * F_step) + G_offset]);
							// skip corresponding tran_G bit if tran_D hasn't been set yet.
							// nasks off the least significant bit if tran_D[f].length == 1
							tran_G_dependency_mask &= (~tran_D[f].length);
						}
						auto& tran_G = block_states[i + j].tran_G;
						decode_tran_vlw(tran_G, (i >> gaggle_size_shift), tran_G_dependency_mask);

						for (ptrdiff_t f = 0; f < F_num; ++f) {
							// bplane_skip handling below works fine because grandhildren 
							// coeffs correspond to one specific subband, different from 
							// handling bplane_skip for other tran words which are 
							// composite.
							if ((tran_G[f].length == 0) & ((bplane_skip & family_masks[(f * F_step) + G_offset]) == 0)) {
								auto& tran_Hx = block_states[i + j].tran_Hx[f];
								decode_tran_vlw.template operator()<symbol_tran_H_codeoption>(tran_Hx, (i >> gaggle_size_shift));
							}
						}
						for (ptrdiff_t f = 0; f < F_num; ++f) {
							// bplane_skip handling below works fine because grandhildren 
							// coeffs correspond to one specific subband, different from 
							// handling bplane_skip for other tran words which are 
							// composite.
							if ((tran_G[f].length == 0) & ((bplane_skip & family_masks[(f * F_step) + G_offset]) == 0)) {
								auto& tran_Hx = block_states[i + j].tran_Hx[f];
								for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
									if (tran_Hx[k].length == 0) {
										uint64_t extract_mask = family_masks[(f * F_step) + Hx_offset + k] & (~bplane_mask[i + j]);
										decode_plain_vlw.template operator()<symbol_types_H_codeoption>((i >> gaggle_size_shift), j, extract_mask);
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
			if constexpr (dbg::bpe::decoder::if_enabled(dbg::bpe::mask_stage_4)) {
			// if constexpr ((dbg::bpe::decoder::disabled_stages & dbg::bpe::mask_stage_4) == 0) {
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

	// TODO: handlers below should also execute on function exit due to exception!
	// 
	{
		auto sign_acc = [](T& dst, uint64_t src) -> void {
			// better be vectorizable, therefore branchless needed
			dst = negpred(dst, src);
		};
		accumulate_bplane<uint64_t, T>(block_signs.data(), output.data.data()->content, output.size * items_per_block, sign_acc);
	}

	// TODO: should be correctly handled on exceptions! see above
	// move to BitPlaneDecoder::decode?
	// 
	// trick segment decoder to handle DC subband as unscaled already
	output.q = relu(((ptrdiff_t)(output.q)) - output.bit_shifts[0]);
	output.bdepthDc = relu(((ptrdiff_t)(output.bdepthDc)) - output.bit_shifts[0]);
	output.bit_shifts[0] = 0;
}

template <typename T>
void BitPlaneDecoder<T>::set_stop_after_DC(bool value) {
	// TODO: implement logic
}

template <typename T>
void BitPlaneDecoder<T>::set_stop_at_bplane(size_t bplane_index, size_t stage_index /* = 0b11 == stage_4 */) {
	// TODO: implement logic
}

//
// see 4.1, p. 4-3, eqs. (12) and (13)
// DCs - twos complement form
// ACs - sign + magnitude
// bdepth(DC) > 0
// bdepth(AC) >= 0
// 
// t(x, b): x => {0, 1, 2, -1} | b
//		t(x, b) == 0 : bit_width(abs(x)) < b
//		t(x, b) == 1 : bit_width(abs(x)) == b
//		t(x, b) == 2 : bit_width(abs(x)) > b
//		t(x, b) == -1 : BitShift(x) > b
// 
//  t_max(Y, b) : max(t(x, b)) | any x in Y
// 
// types(Y, b) : {x & (0x01 << b)} | any x in Y | t(x, b) in {0, 1}
// x & (0x01 << b) <=> t(x, b) | t(x, b) in {0, 1} if x > 0
// AC coefficients are coded as sign + magnitude, therefore x is supposed to be unsigned (e.g. x > 0)
// 
// signs(Y, b) : {sign(x)} | any x in Y | t(x, b) == 1
// 
// tword(L) : {l} | any l=t(x, b) in L | l in {0, 1}
// for a given bitblane b, tword({t(x, b) | x in Y}) == types(Y, b), given x > 0 | any x in Y
//



// tran_B: (B, b) {
//	compute = [](B`, b`) { tword[{t_max(B`, b`)}] }
//  ret = compute(B, b)
//  foreach (b` > b) {
//		if (compute(B, b`) == '1') {
//			ret = null
//			break
//		}
//  }
//	return ret;
// }
// 
// operational:
// (1) null, if ixists b` > b: tran_B(b) == 1
// (2) tword[{t_max(B`, b`)}]
// 
// tran_B is computed beginning with the most significant bitplane, never skipped. 
// tran_B is one bit length at most, possible values are: { null, '0', '1' }. 
// When t_max(B, b) == 2, or t_max(B, b) == -1, then tran_B = null (due to tword)
// Therefore values for tran_B are known for all the previous (more significant) bitplanes 
// when processing current bitplane b. For the most significant bitplane, tran_B can evaluate 
// to either '0' or '1', depending on the corresponding AC values (or bdepthAcBlock). If the 
// initial value is '1', then tran_B is null for all the consequent bitplanes due to rule (1).
// If the initial value is '0', for some less significant bitplane tran_B will necessarily 
// evaluate to '1', that would mean selection of one of the coefficients in the current bitplane 
// (noted as fcoeffac below). After fcoeffac is selected, type of fcoeffac necessarily equals to
// 2, and tran_B for consequtive bitplanes is null per rule (1).
// Typical value transition for tran_B from more significant bit plane to less signinficant one:
// ['0' ->] '1' -> null
// In general, tran_B == '1' means that there is the first AC in joint descendants set to be 
// selected, and it's time to start tracking tran_D to understand what this AC is and when the 
// next AC will be selected.
// 
// 
// 
// 
// tran_D: (B, b) {
//  S = {}
//  foreach (D in B) {
//		d_val = t_max(D, b)
//		foreach (b` > b) {
//			if (t_max(D, b`) == 1) {
//				d_val = null
//			}
//		}
//		S.append(d_val)
//  }
//  return S
// }
// 
// operational:
// (1) tword[{t_max(D0, b), t_max(D1, b), t_max(D2, b)}] | Di: t_max(Di, b`) != 1 | every b` > b
// 
// tran_D is computed beginning with the bitplane for which tran_B evaluates to '1'. For
// previous bitplanes tran_D is null.
// tran_D is 3 bits at most; possible values are:
// { null, {'001', '010', '011', '100', '101', '110', '111'}, {'00', '01', '10', '11'}, {'0', '1'} }
// (the set above is ordered so that values that comes later cannot appear in the stream before 
// the values that comes earlier in the order of the set, and vice-versa (except null, that can be 
// the last element also))
// tran_D indicates what descendants set has the first AC coefficient to be selected in the current
// bitplane. While tran_B equals '0', all AC in all descendants set in the current bitplane are 
// nesessarily zeroes, therefore tran_D is not encoded when tran_B equals '0'. When tran_B is '1', 
// one bit in tran_D necessarily has value '1', and tran_D has length 3 for that bitplane only; 
// for the next bitplane, the length of tran_D is atmost 2; the length is decreased by the number of 
// '1' bits in tran_D for the previous bitplane.
// Typical transition of length for tran_D:
// [0 ->] 3 -> [2 -> [1 ->]] 0
// In general, bit '1' in tran_D means that there is the first AC in the corresponding descendants set 
// to be selected, and it's time to start tracking types(Ci) and tran_G to understand what this AC 
// is and when the next AC will be selected.
// 
// 
// 
// 
// tran_G: (G, b) {
// }
// 
// operational:
// (1) tword[{t_max(G0, b), t_max(G1, b), t_max(G2, b)}] | Gi: exists b` >= b: t_max(Di, b) > 0 
// 
// tran_G is computed beginning with the bitplane for which any bit in tran_D evaluates to '1', 
// that is equivalent to tran_B evaluates to '1'. For previous bitplanes tran_G is null.
// tran_G is 3 bits at most; possible values are:
// { null, '0', '1', '00', '01', '10', '11', '000', '001', '010', '011', '100', '101', '110', '111' }
// The lenght of tran_G is defined by amount of '1' bits set in tran_D for the current and the 
// previous bitplanes and the current types of the coefficients in corresponding G sets. The 
// length of tran_G is never greater than (3 - len(tran_D, b+1)) the value inversely proportional
// to the length of tran_D for the next bitplane. In general, length of tran_G is not decreased 
// monotonously: it is possible that the length of tran_G can be increased if grandchildren
// generation of AC coefficients for some family is selected several bitplanes earlier than 
// grandchildren generation of another family.
// tran_G provides hint on whether setting of corresponding '1' bit in tran_D is caused by the
// corresponding grandchildren set coefficients. In addition, it indicates if it is needed to 
// track and analyze tran_H words.
// State transitions for tran_G are complex. Below is a scheme for possible transitions of 
// length of tran_G:
// 
//              {remains 3}                                       {remains 2}                                 {remains 1}
// [0] -+-----> 1 -+-> ({goto remains 2} | 0 {end})               [0 -+->] 1 -+-> ({remains 1} | 0 {end})     [0 ->] 1 -> 0 {end}
//      |   +------+                                          +-------+<------+
//      |   |                                                 |    
//      +---+-> 2 -+-> ([1 ->] {goto remains 1} | 0 {end})    +-> 2 -> [1 ->] 0 {end}
//      | +-|------+   
//      | | |  
//      +-+>+-> 3 -> [2 -> [1 ->] | 1 ->] 0 {end}
// 
// 
// Well, transition t_max(Gi) 2 -> -1 -> { 0, 1, 2} is not possible per specificaitons in the 
// standard. Type -1 is assigned due to subband weight factor only, and the weight coefficient 
// is set for the whole subband => for the whole generation in a family; i.e. if any coefficient 
// in a generation set has type -1, then all coefficients in that generation are necessary of 
// type -1 also.
//
// 
// 
// 
// template <i>
// tran_H: (H<i>, b) {
// }
// 
// operational:
// (1): tran_H_i = tword[{t_max(H_i0, b), t_max(H_i1, b), t_max(H_i2, b), t_max(H_i3, b)}] | i: { 0, 1, 2, 3 }
// i.e.
// :	tran_H_0 = tword[{t_max(H_00, b), t_max(H_01, b), t_max(H_02, b), t_max(H_03, b)}]
//  	tran_H_1 = tword[{t_max(H_10, b), t_max(H_11, b), t_max(H_12, b), t_max(H_13, b)}]
//  	tran_H_2 = tword[{t_max(H_20, b), t_max(H_21, b), t_max(H_22, b), t_max(H_23, b)}]
//  	tran_H_3 = tword[{t_max(H_30, b), t_max(H_31, b), t_max(H_32, b), t_max(H_33, b)}]
// 
// tran_H_i is 4 separate words:tran_H_0, tran_H_1, tran_H_2, tran_H_3. Maximum length
// of each word if 4 bits. Possible values are any combination of '0' and '1' bits for 
// the given length.
// Each '1' bit in each tran_H_i indicates that it is needed to check types[Hij].
//


// explicit instantiation section

// encoders instantiated only for machine word size output bit streams
template class BitPlaneEncoder<int8_t>;
template void BitPlaneEncoder<int8_t>::encode<uintptr_t>(segment<int8_t>& input, obitwrapper<uintptr_t>& output);
template void BitPlaneEncoder<int8_t>::kOptimal<int8_t, uintptr_t>(kParams<int8_t> params, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int8_t>::kHeuristic<int8_t, uintptr_t>(kParams<int8_t> params, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int8_t>::kEncode<int8_t, uintptr_t>(kParams<int8_t> params, bool use_heuristic, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int8_t>::encodeBpeStages<uintptr_t>(segment<int8_t>& input, obitwrapper<uintptr_t>& output);

template class BitPlaneEncoder<int16_t>;
template void BitPlaneEncoder<int16_t>::encode<uintptr_t>(segment<int16_t>& input, obitwrapper<uintptr_t>& output);
template void BitPlaneEncoder<int16_t>::kOptimal<int16_t, uintptr_t>(kParams<int16_t> params, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int16_t>::kHeuristic<int16_t, uintptr_t>(kParams<int16_t> params, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int16_t>::kEncode<int16_t, uintptr_t>(kParams<int16_t> params, bool use_heuristic, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int16_t>::encodeBpeStages<uintptr_t>(segment<int16_t>& input, obitwrapper<uintptr_t>& output);

template class BitPlaneEncoder<int32_t>;
template void BitPlaneEncoder<int32_t>::encode<uintptr_t>(segment<int32_t>& input, obitwrapper<uintptr_t>& output);
template void BitPlaneEncoder<int32_t>::kOptimal<int32_t, uintptr_t>(kParams<int32_t> params, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int32_t>::kHeuristic<int32_t, uintptr_t>(kParams<int32_t> params, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int32_t>::kEncode<int32_t, uintptr_t>(kParams<int32_t> params, bool use_heuristic, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int32_t>::encodeBpeStages<uintptr_t>(segment<int32_t>& input, obitwrapper<uintptr_t>& output);

template class BitPlaneEncoder<int64_t>;
template void BitPlaneEncoder<int64_t>::encode<uintptr_t>(segment<int64_t>& input, obitwrapper<uintptr_t>& output);
template void BitPlaneEncoder<int64_t>::kOptimal<int64_t, uintptr_t>(kParams<int64_t> params, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int64_t>::kHeuristic<int64_t, uintptr_t>(kParams<int64_t> params, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int64_t>::kEncode<int64_t, uintptr_t>(kParams<int64_t> params, bool use_heuristic, obitwrapper<uintptr_t>& output_stream);
template void BitPlaneEncoder<int64_t>::encodeBpeStages<uintptr_t>(segment<int64_t>& input, obitwrapper<uintptr_t>& output);


// decoders instantiated for 8,32,64 bit input bit streams for every segment data item size
template class BitPlaneDecoder<int8_t>;
template void BitPlaneDecoder<int8_t>::decode<uint8_t>(segment<int8_t>& output, ibitwrapper<uint8_t>& input);
template void BitPlaneDecoder<int8_t>::kReverse<int8_t, uint8_t>(kParams<int8_t>& params, ibitwrapper<uint8_t>& input_stream);
template void BitPlaneDecoder<int8_t>::kDecode<int8_t, uint8_t>(kParams<int8_t>& params, ibitwrapper<uint8_t>& input_stream);
template void BitPlaneDecoder<int8_t>::decodeBpeStages<uint8_t>(segment<int8_t>& output, ibitwrapper<uint8_t>& input);
template void BitPlaneDecoder<int8_t>::decode<uint32_t>(segment<int8_t>& output, ibitwrapper<uint32_t>& input);
template void BitPlaneDecoder<int8_t>::kReverse<int8_t, uint32_t>(kParams<int8_t>& params, ibitwrapper<uint32_t>& input_stream);
template void BitPlaneDecoder<int8_t>::kDecode<int8_t, uint32_t>(kParams<int8_t>& params, ibitwrapper<uint32_t>& input_stream);
template void BitPlaneDecoder<int8_t>::decodeBpeStages<uint32_t>(segment<int8_t>& output, ibitwrapper<uint32_t>& input);
template void BitPlaneDecoder<int8_t>::decode<uint64_t>(segment<int8_t>& output, ibitwrapper<uint64_t>& input);
template void BitPlaneDecoder<int8_t>::kReverse<int8_t, uint64_t>(kParams<int8_t>& params, ibitwrapper<uint64_t>& input_stream);
template void BitPlaneDecoder<int8_t>::kDecode<int8_t, uint64_t>(kParams<int8_t>& params, ibitwrapper<uint64_t>& input_stream);
template void BitPlaneDecoder<int8_t>::decodeBpeStages<uint64_t>(segment<int8_t>& output, ibitwrapper<uint64_t>& input);

template class BitPlaneDecoder<int16_t>;
template void BitPlaneDecoder<int16_t>::decode<uint8_t>(segment<int16_t>& output, ibitwrapper<uint8_t>& input);
template void BitPlaneDecoder<int16_t>::kReverse<int16_t, uint8_t>(kParams<int16_t>& params, ibitwrapper<uint8_t>& input_stream);
template void BitPlaneDecoder<int16_t>::kDecode<int16_t, uint8_t>(kParams<int16_t>& params, ibitwrapper<uint8_t>& input_stream);
template void BitPlaneDecoder<int16_t>::decodeBpeStages<uint8_t>(segment<int16_t>& output, ibitwrapper<uint8_t>& input);
template void BitPlaneDecoder<int16_t>::decode<uint32_t>(segment<int16_t>& output, ibitwrapper<uint32_t>& input);
template void BitPlaneDecoder<int16_t>::kReverse<int16_t, uint32_t>(kParams<int16_t>& params, ibitwrapper<uint32_t>& input_stream);
template void BitPlaneDecoder<int16_t>::kDecode<int16_t, uint32_t>(kParams<int16_t>& params, ibitwrapper<uint32_t>& input_stream);
template void BitPlaneDecoder<int16_t>::decodeBpeStages<uint32_t>(segment<int16_t>& output, ibitwrapper<uint32_t>& input);
template void BitPlaneDecoder<int16_t>::decode<uint64_t>(segment<int16_t>& output, ibitwrapper<uint64_t>& input);
template void BitPlaneDecoder<int16_t>::kReverse<int16_t, uint64_t>(kParams<int16_t>& params, ibitwrapper<uint64_t>& input_stream);
template void BitPlaneDecoder<int16_t>::kDecode<int16_t, uint64_t>(kParams<int16_t>& params, ibitwrapper<uint64_t>& input_stream);
template void BitPlaneDecoder<int16_t>::decodeBpeStages<uint64_t>(segment<int16_t>& output, ibitwrapper<uint64_t>& input);

template class BitPlaneDecoder<int32_t>;
template void BitPlaneDecoder<int32_t>::decode<uint8_t>(segment<int32_t>& output, ibitwrapper<uint8_t>& input);
template void BitPlaneDecoder<int32_t>::kReverse<int32_t, uint8_t>(kParams<int32_t>& params, ibitwrapper<uint8_t>& input_stream);
template void BitPlaneDecoder<int32_t>::kDecode<int32_t, uint8_t>(kParams<int32_t>& params, ibitwrapper<uint8_t>& input_stream);
template void BitPlaneDecoder<int32_t>::decodeBpeStages<uint8_t>(segment<int32_t>& output, ibitwrapper<uint8_t>& input);
template void BitPlaneDecoder<int32_t>::decode<uint32_t>(segment<int32_t>& output, ibitwrapper<uint32_t>& input);
template void BitPlaneDecoder<int32_t>::kReverse<int32_t, uint32_t>(kParams<int32_t>& params, ibitwrapper<uint32_t>& input_stream);
template void BitPlaneDecoder<int32_t>::kDecode<int32_t, uint32_t>(kParams<int32_t>& params, ibitwrapper<uint32_t>& input_stream);
template void BitPlaneDecoder<int32_t>::decodeBpeStages<uint32_t>(segment<int32_t>& output, ibitwrapper<uint32_t>& input);
template void BitPlaneDecoder<int32_t>::decode<uint64_t>(segment<int32_t>& output, ibitwrapper<uint64_t>& input);
template void BitPlaneDecoder<int32_t>::kReverse<int32_t, uint64_t>(kParams<int32_t>& params, ibitwrapper<uint64_t>& input_stream);
template void BitPlaneDecoder<int32_t>::kDecode<int32_t, uint64_t>(kParams<int32_t>& params, ibitwrapper<uint64_t>& input_stream);
template void BitPlaneDecoder<int32_t>::decodeBpeStages<uint64_t>(segment<int32_t>& output, ibitwrapper<uint64_t>& input);

template class BitPlaneDecoder<int64_t>;
template void BitPlaneDecoder<int64_t>::decode<uint8_t>(segment<int64_t>& output, ibitwrapper<uint8_t>& input);
template void BitPlaneDecoder<int64_t>::kReverse<int64_t, uint8_t>(kParams<int64_t>& params, ibitwrapper<uint8_t>& input_stream);
template void BitPlaneDecoder<int64_t>::kDecode<int64_t, uint8_t>(kParams<int64_t>& params, ibitwrapper<uint8_t>& input_stream);
template void BitPlaneDecoder<int64_t>::decodeBpeStages<uint8_t>(segment<int64_t>& output, ibitwrapper<uint8_t>& input);
template void BitPlaneDecoder<int64_t>::decode<uint32_t>(segment<int64_t>& output, ibitwrapper<uint32_t>& input);
template void BitPlaneDecoder<int64_t>::kReverse<int64_t, uint32_t>(kParams<int64_t>& params, ibitwrapper<uint32_t>& input_stream);
template void BitPlaneDecoder<int64_t>::kDecode<int64_t, uint32_t>(kParams<int64_t>& params, ibitwrapper<uint32_t>& input_stream);
template void BitPlaneDecoder<int64_t>::decodeBpeStages<uint32_t>(segment<int64_t>& output, ibitwrapper<uint32_t>& input);
template void BitPlaneDecoder<int64_t>::decode<uint64_t>(segment<int64_t>& output, ibitwrapper<uint64_t>& input);
template void BitPlaneDecoder<int64_t>::kReverse<int64_t, uint64_t>(kParams<int64_t>& params, ibitwrapper<uint64_t>& input_stream);
template void BitPlaneDecoder<int64_t>::kDecode<int64_t, uint64_t>(kParams<int64_t>& params, ibitwrapper<uint64_t>& input_stream);
template void BitPlaneDecoder<int64_t>::decodeBpeStages<uint64_t>(segment<int64_t>& output, ibitwrapper<uint64_t>& input);
