#pragma once

// TODO: refactor include paths
#include "core_types.h"
#include "utils.h"

#include "obitwrapper.tpp"

#include <array>
#include <vector>
#include <bit>

template <typename T, size_t allignment>
class BitPlaneEncoder {
	void encodeDc(const segment<T>& input);

private:
	void kHeuristic(const segment<T>& input, obitwrapper<size_t>& output_stream);
	void kOptimal(const segment<T>& input, obitwrapper<size_t>& output_stream);

	obitwrapper<T> obitbuffer; // check if is owned or passed through the calls as parameter
};

template <typename T, size_t allignment>
void BitPlaneEncoder<T, allignment>::kOptimal(const segment<T>& input, obitwrapper<size_t>& output_stream) {
	// TODO: check N=1 option
	// implement heuristic k computation
	//
	// segment<T> input;
	// obitwrapper<size_t> output_stream;

	constexpr size_t max_N_value = 10;
	size_t N = input.bdepthDc - input.q; // N is expected to be in range [1, 10]
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

	constexpr size_t gaggle_len = 16;
	constexpr size_t gaggle_scale_factor = 2;

	// zero placehoder for DC reference samble. 
	// zero remaining placeholders for dc elements at the end of the segmnet to extend 
	// the last gaggle to len=16
	{
		input.quantizedDc[-1] = 0;
		constexpr size_t gaggle_mask = gaggle_len - 1;
		ptrdiff_t dc_boundary = input.size + ((~input.size) & gaggle_mask);
		for (ptrdiff_t i = input.size; i < dc_boundary; ++i) {
			input.quantizedDc[i] = 0;
		}
	}

	constexpr size_t k_simd_width = 8;
	std::vector<std::pair<size_t, ptrdiff_t>> k_options((input.size >> 4) + (gaggle_scale_factor * 2)); // extended just in case

	uint_least16_t simdr_k_bound_mask[k_simd_width] = { 0 };
	for (ptrdiff_t i = k_bound; i < k_simd_width; ++i) {
		// clear sign bit to make comparison operate correctly later
		simdr_k_bound_mask[i] = ((uint_least16_t)((ptrdiff_t)(-1))) >> 1;
	}
	constexpr uint_least16_t simdr_shift[k_simd_width] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	// requires input.quantizedDc to have gaggle_len * gaggle_scale_factor additional elements at the end (at most)
	for (ptrdiff_t i = 0; i < input.size; i += gaggle_len * gaggle_scale_factor) {
		uint_least16_t simdr_acc[gaggle_scale_factor][k_simd_width] = { 0 };
		uint_fast16_t gpr_acc[gaggle_scale_factor] = { 0 };

		// attempt to increase pipline load uniformity; repeat uniform operation blocks several times
		// should be beneficial for branch prediction in the second block as well
		for (ptrdiff_t j = 0; j < gaggle_scale_factor; ++j) {
			for (ptrdiff_t ii = 0; ii < gaggle_len; ++ii) {
				uint_fast16_t gpr = input.quantizedDc[i + (j * gaggle_len) + ii - 1]; // -1 here because of reference sample
				gpr_acc[j] += gpr;
				uint_least16_t simdr[k_simd_width];
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

		ptrdiff_t k_option_index = i << 4;
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
					current_k.second = simdr_shift[j][jj];
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
		output_stream << {k_bitsize, k_options[gaggle_index].second};
		output_stream << {N, input.referenceSample};

		ptrdiff_t i;
		bool execute_preamble = false;
		for (i = -1; i < input.size - gaggle_len; i += gaggle_len) {
			if (execute_preamble) {
				gaggle_index = (i >> 4) + 1;
				if (k_options[gaggle_index].first >= k_uncoded_len) {
					k_options[gaggle_index].second = k_uncoded_opt;
				}
				output_stream << {k_bitsize, k_options[gaggle_index].second};
			}
			execute_preamble = true;

			if (k_options[gaggle_index].second != k_uncoded_opt) {
				for (ptrdiff_t j = (i < 0); j < gaggle_len; ++j) {
					output_stream << {(input.quantizedDc[i + j] >> k_options[gaggle_index].second) + 1, 1};
				}
			} else {
				// WTF is that? I don't remeber the reason I wrote this code
				// is it casting signed k value to unsigned alternative of k encoding?
				// k_options[gaggle_index].second = (~(((ptrdiff_t)(-1)) << k_bitsize)) + 1;
				k_options[gaggle_index].second = N;
			}

			for (ptrdiff_t j = (i < 0); j < gaggle_len; ++j) {
				output_stream << {k_options[gaggle_index].second, input.quantizedDc[i + j]};
			}
		}

		ptrdiff_t last_gaggle_len = input.size - i;
		size_t k_uncoded_len_last = N * last_gaggle_len - last_gaggle_len;
		if (execute_preamble) {
			gaggle_index = (i >> 4) + 1;
			if (k_options[gaggle_index].first >= k_uncoded_len_last) {
				k_options[gaggle_index].second = k_uncoded_opt;
			}
			output_stream << {k_bitsize, k_options[gaggle_index].second};
		}
		execute_preamble = true;

		if (k_options[gaggle_index].second != k_uncoded_opt) {
			for (ptrdiff_t j = (i < 0); j < last_gaggle_len; ++j) {
				output_stream << {(input.quantizedDc[i + j] >> k_options[gaggle_index].second) + 1, 1};
			}
		} else {
			k_options[gaggle_index].second = N;
		}

		for (ptrdiff_t j = (i < 0); j < last_gaggle_len; ++j) {
			output_stream << {k_options[gaggle_index].second, input.quantizedDc[i + j]};
		}
	}
}

template <typename T, size_t allignment>
void BitPlaneEncoder<T, allignment>::kHeuristic(const segment<T>& input, obitwrapper<size_t>& output_stream) {
	size_t N = input.bdepthDc - input.q; // N is expected to be in range [1, 10]
	size_t k_bitsize = std::bit_width(N - 1);
	constexpr ptrdiff_t k_uncoded_option = -1;
	constexpr size_t gaggle_len = 16;
	ptrdiff_t J = 15;
	for (ptrdiff_t i = 0; i < input.size; i += J) {
		if (i > 0) {
			J = std::min(gaggle_len, input.size - i);
		}
		size_t delta = 0;
		for (ptrdiff_t j = 0; j < J; ++j) {
			delta += input.quantizedDc[i + j];
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
		if (predicate1[0] >= predicate2[0]) {
			k = k_uncoded_option;
		} else if (predicate1[1] > predicate2[1]) {
			k = 0;
		} else if (predicate1[2] <= predicate2[2]) {
			k = N - 2;
		} else {
			// TODO: check underflow/arithmetic/input values, potential UB
			k = std::bit_width(predicate2[2] / J) - 7;
		}

		output_stream << { k_bitsize, k };
		if (i == 0) {
			output_stream << { N, input.referenceSample };
		}
		if (k != k_uncoded_option) {
			for (ptrdiff_t j = 0; j < J; ++j) {
				output_stream << { (input.quanitzedDc[i + j] >> k) + 1, 1 };
			}
		} else {
			k = N;
		}
		for (ptrdiff_t j = 0; j < J; ++j) {
			output_stream << { k, input.qiantizedDc[i + j]};
		}
	}
}

template <typename T, size_t allignment>
void BitPlaneEncoder<T, allignment>::encodeDc(const segment<T>& input) {
	obitwrapper<size_t> output_stream;
	bool use_heuristic;

	// TODO: check if std::max is needed with 1 as a second parameter for N computation
	size_t N = input.bdepthDc - input.q; // N is expected to be in range [1, 10]
	if (N > 1) {
		if (use_heuristic) {
			this->kHeuristic(input, output_stream);
		} else {
			this->kOptimal(input, output_stream);
		}
	} else {
		// N == 1
		for (ptrdiff_t i = 0; i < input.size; ++i) {
			output_stream << { N, input.quantizedDc[i] };
		}
	}


}

#include <type_traits>

void temp_experiment_function() {
	typedef size_t item_t;
	item_t a = 5;
	item_t b = a ^ (a << 1);

	item_t c = ((std::make_signed_t<item_t>)(-1)) ^ b;
	item_t d = c + 1;



	// for (ptrdiff_t k = 0; k <= k_bound; ++k) {
	// 	obitwrapper<T> trial_buffer;
	// 	size_t qDc_count = relu(((ptrdiff_t)(input.size)) - 1); // reference sample is not included
	// 	for (ptrdiff_t i = 1; i < qDc_count; ++i) {
	// 		trial_buffer << shift_params{ .code = 1, .length = input.quantizedDc[i] + 1 };
	// 	}
	// }

	// uint_least16_t simdr_acc[k_simd_width] = { 0 };
	// uint_fast16_t gpr_acc = 0;
	// for (ptrdiff_t j = 0; j < gaggle_len; ++j) {
	// 	uint_fast16_t gpr = input.quantizedDc[i + j];
	// 	gpr_acc += gpr;
	// 	uint_least16_t simdr[k_simd_width];
	// 	for (ptrdiff_t ii = 0; ii < k_simd_width; ++ii) {
	// 		simdr[ii] = gpr;
	// 		simdr[ii] >>= simdr_shift[ii];
	// 		simdr_acc[ii] += simdr[ii];
	// 	}
	// }
	// for (ptrdiff_t j = 0; j < k_simd_width; ++j) {
	// 	simdr_acc[j] |= simdr_k_bound_mask[j];
	// }

	// ptrdiff_t gaggle_size = 15;
	// k_options[0].first = k_options[0].first - 1 - N; // to make compare below correct
	// output_stream << {N, input.referenceSample};
	// 
	// 
	// size_t k_bitsize = std::bit_width(N - 1);
	// for (ptrdiff_t i = 0; i < input.size - gaggle_len; i += gaggle_size, gaggle_size = gaggle_len) {
	// 	ptrdiff_t gaggle_index = i >> 4;
	// 	output_stream << {k_bitsize, k_options[gaggle_index].second};
	// 	if (k_options[gaggle_index].second != k_uncoded_opt) {
	// 		for (ptrdiff_t j = 0; j < gaggle_size; ++j) {
	// 			output_stream << {(input.quantizedDc[i + j] >> k_options[gaggle_index].second) + 1, 1};
	// 		}
	// 	}
	// 	else {
	// 		k_options[gaggle_index].second = (~(((ptrdiff_t)(-1)) << k_bitsize)) + 1;
	// 	}
	// 	for (ptrdiff_t j = 0; j < gaggle_size; ++j) {
	// 		output_stream << {k_options[gaggle_index].second, input.quantizedDc[i + j]};
	// 	}
	// }


	// ptrdiff_t k_option_index = i << 4;
	// for (ptrdiff_t j = 0; j < gaggle_scale_factor; ++j) {
	// 	// There is no straitforward way to compare with uncoded option reliably due to different 
	// 	// uncoded stream length for the first gaggle, subsequent gaggles and the last one. 
	// 	// Encoded stream lengths can be compared safely on the other hand as the diff value due to
	// 	// additional '1' bit is the same for all k options for a given gaggle.
	// 	decltype(k_options)::value_type current_k = { k_uncoded_len, k_uncoded_opt };
	// 	if (gpr_acc[j] < current_k.first) {
	// 		current_k.first = gpr_acc[j];
	// 		current_k.second = 0;
	// 	}
	// 	// hopefully this can be vectorized or at least well-predicted
	// 	for (ptrdiff_t jj = 0; jj < k_simd_width; ++jj) {
	// 		if (simdr_acc[j][jj] < current_k.first) {
	// 			current_k.first = simdr_acc[j][jj];
	// 			current_k.second = simdr_shift[j][jj];
	// 		}
	// 	}
	// 	k_options[k_option_index + j] = current_k;
	// }
}



