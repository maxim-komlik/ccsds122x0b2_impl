#pragma once

// TODO: refactor include paths
#include "core_types.h"
#include "utils.h"

#include "obitwrapper.tpp"

#include <array>
#include <vector>
#include <deque>
#include <bit>

template <typename T>
struct kParams {
	T* data; 
	size_t datalength;
	size_t bdepth;
	T reference;
};

template <typename T>
inline vlw_t combine_bword(T raw, T select_mask);

template <typename T, typename obwT, size_t alignment = 16>
void bplane(T* data, size_t size, size_t b, obitwrapper<obwT>& ostream);

template <typename T, typename obwT, size_t alignment = 16>
void bplanev4(T* data, size_t size, size_t belder, std::array<std::reference_wrapper<obitwrapper<obwT>>, 4> ostreams);

template <typename T, size_t alignment>
class BitPlaneEncoder {
	void encodeDc(const segment<T>& input);

private:
	void kHeuristic(const segment<T>& input, obitwrapper<size_t>& output_stream);
	void kOptimal(const segment<T>& input, obitwrapper<size_t>& output_stream);

	obitwrapper<T> obitbuffer; // check if is owned or passed through the calls as parameter
};

template <typename T, typename obwT, size_t alignment = 16>
void kOptimal(kParams<T> params, obitwrapper<obwT>& output_stream) {
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

	constexpr size_t gaggle_len = 16;
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
		for (i = -1; i < params.datalength - gaggle_len; i += gaggle_len) {
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
				// WTF is that? I don't remeber the reason I wrote this code
				// is it casting signed k value to unsigned alternative of k encoding?
				// k_options[gaggle_index].second = (~(((ptrdiff_t)(-1)) << k_bitsize)) + 1;
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

template <typename T, typename obwT, size_t alignment = 16>
void kHeuristic(kParams<T> params, obitwrapper<obwT>& output_stream) {
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

template <typename T, typename obwT, size_t alignment = 16>
inline void kEncode(kParams<T> params, bool use_heuristic, obitwrapper<obwT>& output_stream) {
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
		for (ptrdiff_t i = 0; i < params.datalength; ++i) {
			output_stream << vlw_t{ params.bdepth, (size_t)(params.data[i]) };
		}
	}
}

template <typename T, typename obwT, size_t alignment = 16>
void bplaneEncode(T* data, size_t datasize, size_t pindex, size_t pcount, obitwrapper<obwT>& boutput) {
	// TODO: underlying obitwrapper type
	constexpr size_t bufferbsize = sizeof(obwT) << 3;
	// TODO: Probably intializing temporal buffers with optimal size_t underlying type is desirable, to make sure
	// that forwarding content from temporal buffers to the output bit stream is efficient. That requires
	// initializing array of deque<size_t> and then passing tuple with the first element of type corresponding
	// to the output bitstream, and the rest 3 temporal bit streams as an array of bitstreams with type size_t
	//
	std::array<std::deque<obwT>, 3> vobuffers;
	auto ocallback = [&] <ptrdiff_t i> (obwT item) -> void {
		vobuffers[i].push_back(item);
	};
	std::array<obitwrapper<obwT>, 3> vwrappers {
		obitwrapper<obwT>(std::bind(&decltype(ocallback)::template operator()<0>, &ocallback, std::placeholders::_1)),
		obitwrapper<obwT>(std::bind(&decltype(ocallback)::template operator()<1>, &ocallback, std::placeholders::_1)),
		obitwrapper<obwT>(std::bind(&decltype(ocallback)::template operator()<2>, &ocallback, std::placeholders::_1))
	};
	std::array<std::reference_wrapper<obitwrapper<obwT>>, 4> vboutputs = {
		std::ref(boutput),
		std::ref(vwrappers[0]),
		std::ref(vwrappers[1]),
		std::ref(vwrappers[2])
	};

	while (pcount > 0) {
		size_t pstep = std::min(pcount, (decltype(pcount))(4));
		if (pstep > 1) {
			bplanev4(data, datasize, pindex, vboutputs);
			for (ptrdiff_t i = 1; i < pstep; ++i) {
				ptrdiff_t buffer_index = i - 1;
				if (boutput.dirty()) {
					// executes arithmetic shifts inside obitwrapper::operator<< with shift size
					// equal bitsize of an underlying type, resulting in UB (riscv and x86 limit
					// shift amount to 5 bit for 32-bit and to 6 bit for 64-bit integrals) if 
					// boutput is clean and boutput's underlying type is at least the the same 
					// width as the current buffer's one
					while (!(vobuffers[buffer_index].empty())) {
						boutput << vlw_t{ bufferbsize, vobuffers[buffer_index].front() };
						vobuffers[buffer_index].pop_front();
					}
				} else {
					while (!(vobuffers[buffer_index].empty())) {
						boutput.flush_word(vobuffers[buffer_index].front());
						vobuffers[buffer_index].pop_front();
					}
				}

				size_t bufshift = vboutputs[i].get().ocount();
				// The branch below should be always hit because of unconditional flush. But it is handled 
				// properly because 0-length word is inserted into the stream if the buffer was empty before
				// flashing. Yet deque::pop_front() is UB when called on non-empty container, may make sense
				// to keep branch and mark it as [[likely]] and handle errors in else branch (but should never
				// happen).
				/// vboutputs[i].get().flush();
				/// boutput << vlw_t{ bufferbsize - bufshift, (vobuffers[buffer_index].front() >> bufshift) };
				/// vobuffers[buffer_index].pop_front();
				vboutputs[i].get().flush();
				[[likely]]
				if (!(vobuffers[buffer_index].empty())) {
					boutput << vlw_t{ bufferbsize - bufshift, (vobuffers[buffer_index].front() >> bufshift) };
					vobuffers[buffer_index].pop_front();
				} else {
					// should never happen
					throw "UB!";
				}
			}
		} else {
			bplane(data, datasize, pindex, boutput);
		}
		pindex -= pstep;
		pcount -= pstep;
	}
}

template <typename T, size_t alignment>
void __encode(segment<T> input) {
	// TODO: check if needed to implement input segment validation (input.size > 1?)
	std::vector<uint64_t> bpe_debug_output_buffer;
	auto bpe_debug_output_buffer_callback = [&bpe_debug_output_buffer](uint64_t item) -> void {
		bpe_debug_output_buffer.push_back(item);
	};
	// TODO: callback ownership problem in bitwrapper
	// std::function<void(uint64_t)> temp_func_wrapper{ bpe_debug_output_buffer_callback };
	// obitwrapper output(temp_func_wrapper);
	obitwrapper<uint64_t> output(bpe_debug_output_buffer_callback);
	bool use_heuristic_DC = false;
	bool use_heuristic_bdepthAc = false;

	constexpr size_t max_subband_shift = 3;

	kParams quantizedDcParams = {
		input.quantizedDc.data(),
		input.size - 1,	// -1 because of a reference sample that is not part of the vector
		input.bdepthDc - input.q, 
		input.referenceSample
	}; 
	kEncode(quantizedDcParams, use_heuristic_DC, output);

	size_t bit_shift_LL3 = input.bit_shifts[0];
	size_t additional_bdepth = std::max(input.bdepthAc, bit_shift_LL3);
	if (input.q > additional_bdepth) {
		bplaneEncode(input.plainDc.data(), input.size, input.q, input.q - additional_bdepth, output);
	}
	
	if (input.bdepthAc > 0) {
		kParams quantizedBdepthAcParams = {
			input.quantizedBdepthAc.data(),
			input.size - 1,	// -1 because of a reference sample that is not part of the vector
			std::bit_width(input.bdepthAc),
			input.referenceBdepthAc
		};
		kEncode(quantizedBdepthAcParams, use_heuristic_bdepthAc, output);
		__encodeBpeStages<T, alignment>(input, output);
	}
}

class tempsymbolstream {
	using content_t = dense_vlw_t::type;
	std::vector<content_t> content;
	size_t size = 0;
	size_t currentIndex = 0;

public:
	~tempsymbolstream() = default;
	tempsymbolstream() = default;

	tempsymbolstream(std::vector<content_t>&& src) :
			content(std::move(src)), size(0), currentIndex(0) { }

	const content_t* const data() const {
		return this->content.data();
	}

	size_t bytelength() const {
		return this->content.size();
	}

	size_t _size() const {
		return this->size;
	}

	void reset() {
		this->restart();
		this->size = 0;
		this->content.clear();
	}

	void restart() {
		this->currentIndex = 0;
	}

	size_t get() {
		content_t ret_value =
			(((content[this->currentIndex >> 1]) &
				((content_t)((-(int)((~(this->currentIndex)) & 0x01)) ^ 0x0f))) >>
			((-(int)((~(this->currentIndex)) & 0x01)) & 4));
		++(this->currentIndex);
		return (size_t)(ret_value);
	}

	operator bool() { 
		return currentIndex != size; 
	}

	bool operator!() {
		// currentIndex == size;
		return !((bool)*this);
	}

	void put(content_t item) {
		++(this->size);
		if (this->size > (this->content.size() << 1)) {
			this->content.push_back(item << 4);
		} else {
			this->content[(this->size - 1) >> 1] ^= item;
		}
	}
};

#include "symbol_code.tpp"
#include "EntropyCode.tpp"

struct BitOrderTranslator {
private:
	std::tuple <std::array<size_t, (1 << 1)>,
		std::array<size_t, (1 << 2)>,
		std::array<size_t, (1 << 3)>,
		std::array<size_t, (1 << 4)>> codes = 
	{
		{ 0x00, 0x01 }, 
		{ 0x00, 0x02, 0x01, 0x03 }, 
		{ 0x00, 0x04, 0x02, 0x06, 0x01, 0x05, 0x03, 0x07 }, 
		{ 0x00, 0x08, 0x04, 0x0c, 0x02, 0x0a, 0x06, 0x0e, 0x01, 0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f}
	};

	std::array<size_t*, 5> mapping = {
		std::get<0>(codes).data(), 
		std::get<0>(codes).data(), 
		std::get<1>(codes).data(), 
		std::get<2>(codes).data(), 
		std::get<3>(codes).data()
	};
public:
	void translate(dense_vlw_t& vlw) {
		vlw.value = this->mapping[vlw.length][vlw.value];
	}
};

struct SymbolTranslator {
	static const size_t types_H_codeparam = 0x02;
	static const size_t tran_H_codeparam = 0x02;
	static const size_t tran_D_codeparam = 0x01;

	// TODO: private: ?
	std::tuple<
		std::tuple<symbol_code<1, 0>>, // fictive one
		std::tuple<symbol_code<2, 0>>,
		std::tuple<symbol_code<3, 0>, symbol_code<3, 1>>,
		std::tuple<symbol_code<4, 0>, symbol_code<4, 1>>> codes;

	std::array<std::array<size_t*, 5>, 3> mapping = { {
		{
			std::get<0>(std::get<0>(codes)).mapping,
			std::get<0>(std::get<0>(codes)).mapping,
			std::get<0>(std::get<1>(codes)).mapping,
			std::get<0>(std::get<2>(codes)).mapping,
			std::get<0>(std::get<3>(codes)).mapping
		},
		{
			std::get<0>(std::get<0>(codes)).mapping,
			std::get<0>(std::get<0>(codes)).mapping,
			std::get<0>(std::get<1>(codes)).mapping,
			std::get<1>(std::get<2>(codes)).mapping,
			std::get<0>(std::get<3>(codes)).mapping
		},
		{
			std::get<0>(std::get<0>(codes)).mapping,
			std::get<0>(std::get<0>(codes)).mapping,
			std::get<0>(std::get<1>(codes)).mapping,
			std::get<0>(std::get<2>(codes)).mapping,
			std::get<1>(std::get<3>(codes)).mapping
		}
	} };

public:
	template <size_t codeparam = 0>
	void translate(dense_vlw_t& vlw) {
		vlw.value =
			this->mapping[codeparam][vlw.length][vlw.value];
	}
};

struct EnthropyTranlator {
	// TODO: private: ?
	std::tuple<
		std::tuple<EntropyCode<2, 0>>,
		std::tuple<EntropyCode<3, 0>, EntropyCode<3, 1>>,
		std::tuple<EntropyCode<4, 0>, EntropyCode<4, 1>, EntropyCode<4, 2>>> codes;

	std::array<std::array<SymbolEncoding*, 3>, 5> mapping = { {
		{
			nullptr,
			nullptr,
			nullptr
		},
		{
			nullptr,
			nullptr,
			nullptr
		},
		{
			std::get<0>(std::get<0>(codes)).mapping,
			nullptr,
			nullptr
		},
		{
			std::get<0>(std::get<1>(codes)).mapping,
			std::get<1>(std::get<1>(codes)).mapping,
			nullptr
		},
		{
			std::get<0>(std::get<2>(codes)).mapping,
			std::get<1>(std::get<2>(codes)).mapping,
			std::get<2>(std::get<2>(codes)).mapping
		}
	} };

public:
	size_t _word_length(vlw_t& vlw, ptrdiff_t codeoption) {
		ptrdiff_t length = vlw.length;
		if (codeoption != -1) {
			length = this->mapping[length][codeoption][vlw.value].length;
		}
		return length;
	}

	size_t _word_length(size_t code, size_t length, ptrdiff_t codeoption) {
		size_t result = length;
		if (codeoption != -1) {
			result = this->mapping[length][codeoption][code].length;
		}
		return result;
	}

	// TODO: refactor
	vlw_t translate(vlw_t vlw, ptrdiff_t codeoption) {
		SymbolEncoding result {
			.length = vlw.length, 
			.code = vlw.value
		};

		if (codeoption != -1) {
			result = this->mapping[result.length][codeoption][result.code];
		}
		return { result.length, result.code };
	}
};

template <typename T, size_t alignment>
void __encodeBpeStages(segment<T>& input, obitwrapper<uint64_t>& output) {
	constexpr size_t max_subband_shift = 3;
	constexpr size_t items_per_block = 64;	// TODO: global constant as a property of a block struct
	constexpr size_t items_per_gaggle = 16;

	std::vector<uint64_t> block_signs;
	block_signs.reserve(input.size);
	// will be better to extract several bplanes at once, therefore memory 
	// reusage considerations arise. Vector is considered to be not effective 
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
	T* raw_block_data = std::assume_aligned<(alignment << sizeof(T))>(input.data.data()->content);
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

	constexpr size_t F_num = 3;
	constexpr size_t CpF_num = 1;
	constexpr size_t HxpF_num = 4;
	constexpr ptrdiff_t D_offset = 1;
	constexpr ptrdiff_t C_offset = 2;
	constexpr ptrdiff_t G_offset = 3;
	constexpr ptrdiff_t Hx_offset = 4;
	constexpr ptrdiff_t F_step = 8;
	constexpr ptrdiff_t B_index = 0;
	constexpr ptrdiff_t P_index = 8;

	constexpr std::array<uint64_t, 24> family_masks = []() constexpr {
		std::array<uint64_t, 24> output{ 0 };

		constexpr size_t gen_num = 2;
		constexpr ptrdiff_t C_gen_index = 0;
		constexpr ptrdiff_t G_gen_index = 1;

		// constexpr ptrdiff_t reserved_1_index = 8;
		constexpr ptrdiff_t reserved_2_index = 16;

		constexpr uint64_t X_mask = 0x0f;	// 4-bit mask for one group
		constexpr size_t X_mask_blen = 4;

		// for (ptrdiff_t i = (F_num - 1); i >= 0; --i) {
		for (ptrdiff_t i = 0; i < F_num; ++i) {
			// for (ptrdiff_t j = (gen_num - 1); j >= 0; --j) {
			for (ptrdiff_t j = 0; j < gen_num; ++j) {
				if (j == C_gen_index) {
					output[i * F_step + C_offset] =
						(X_mask << ((F_num - i - 1) * CpF_num * X_mask_blen))
							<< (F_num * HxpF_num * X_mask_blen);
				}
				if (j == G_gen_index) {
					// for (ptrdiff_t k = (HxpF_num - 1); k >= 0; --k) {
					for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
						output[i * F_step + Hx_offset + k] =
							(X_mask << ((HxpF_num - k - 1) * X_mask_blen))
								<< ((F_num - i - 1) * HxpF_num * X_mask_blen);
						output[i * F_step + G_offset] |= output[i * F_step + Hx_offset + k];
					}
				}
				output[i * F_step + D_offset] |= output[i * F_step + C_offset + j];
			}
			output[B_index] |= output[i * F_step + D_offset];
		}
		output[P_index] = ((X_mask >> 1) << (F_num * (CpF_num + HxpF_num) * X_mask_blen));
		return output;
	}();


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

		bool encode_descendants = false;
	};

	struct gaggle_bpe_meta {
		std::array<ptrdiff_t, 5> entropy_codeoptions = { -1, -1, -1, -1, -1 };
		std::array<bool, 5> code_marker_written = { true, true, false, false, false };
	};

	SymbolTranslator symbol_translator;
	EnthropyTranlator entropy_translator;

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

	// Maybe better alternative exists like creating temporary, calling reserve but discard reserve result
	//
	auto vector_with_capacity_generator = []<typename T>(size_t capacity) -> std::vector<T> {
		std::vector<T> result;
		result.reserve(capacity);
		return result;
	};

	std::array<tempsymbolstream, 3> vlwstreams {
		tempsymbolstream(vector_with_capacity_generator.template operator()<dense_vlw_t::type>(144)),
		tempsymbolstream(vector_with_capacity_generator.template operator()<dense_vlw_t::type>(128)),
		tempsymbolstream(vector_with_capacity_generator.template operator()<dense_vlw_t::type>(128))
	};


	constexpr size_t gaggle_mask = items_per_gaggle - 1; // 0x0f
	size_t input_size_truncated = input.size & (~gaggle_mask);
	size_t last_gaggle_size = input.size & gaggle_mask;

	size_t b = input.bdepthAc - 1;
	for (; b >= max_subband_shift; --b) {
		// stage 0
		if (b < input.q) {
			// q may be less than bdepthAc, e.g.:
			// bdepthAc = 3; bdepthDc = 10; bitShift(LL3) = 0
			// => 
			// q = 2
			//
			bplaneEncode(input.plainDc.data(), input.size, b, 1, output); // the output buffer can still not be flushed
		}
		// interleaving bpe stages
		{
			if (current_bplane.empty()) {
				// extracts next 4 bplanes at most
				size_t bplane_num_to_extract = std::min(b, (decltype(b))(3));
				++bplane_num_to_extract;

				// fills current_bplane container
				bplaneEncode(raw_block_data, raw_block_size, b, bplane_num_to_extract, current_bitplane_bitwrapper);
			}

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
				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
					std::array<decltype(family_masks)::value_type, family_masks.size()> prepared_block = { 0 };
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

						// tran_len value is either 0 or 1, and used as both mask and len
						vlw.value <<= tran_len;
						vlw.value |= (tran_len & tran_val);
						vlw.length += tran_len;

						return ((tran_val == 0) & (tran_len == 1));
					};

					auto computeTerminalVlw = [&](dense_vlw_t& typesVlw, dense_vlw_t& signsVlw, size_t index, bool skip = false) {
						// All active elements that are not of type 2 (or -1) yet, mask is applicable
						// to get values '0' and '1' of dedicated bits
						uint64_t effective_mask = (family_masks[index] & (~bplane_mask[i + j])) & (-((int64_t)(!skip)));

						typesVlw = combine_bword(prepared_block[index], effective_mask);
						// Actually the same as below, but may relief contention for a single item 
							// in current_bplane[i + j] and use masked copy instead, that should be just
							// used by computing transition word
							// 
							// ) = combine_bword(current_bplane[i + j], effective_mask);
						signsVlw = combine_bword(block_signs[i + j], (prepared_block[index] & effective_mask));

						// block_states[i + j].bplane_mask_transit |= effective_mask;
						block_states[i + j].bplane_mask_transit |= (prepared_block[index] & effective_mask);
					};

					// set tran words to completely clean states to mitigate interfering with leftovers
					// of the previous bitplane. Also allows write operations only with no need to read
					// the old state and elide assosiated memory ops dependencies.
					block_states[i + j].tran_B = dense_vlw_t{ 0, 0 };
					bool skip_descendants = accumulateTranVlw(block_states[i + j].tran_B, B_index);
					computeTerminalVlw(block_states[i + j].types_P, block_states[i + j].signs_P, P_index);
					symbol_translator.translate(block_states[i + j].types_P);
					addVlwToCollection(block_states[i + j].types_P);

					if (!skip_descendants) {
						block_states[i + j].tran_D = dense_vlw_t{ 0, 0 };
						block_states[i + j].tran_G = dense_vlw_t{ 0, 0 };
						for (ptrdiff_t k = 0; k < F_num; ++k) {
							// (skip = false) is implied because the case is covered by control flow dependency
							// introduced by branch few lines above
							bool skip_tran_Gx = accumulateTranVlw(block_states[i + j].tran_D, (k * F_step) + D_offset);
							bool skip_tran_Hx = accumulateTranVlw(block_states[i + j].tran_G, (k * F_step) + G_offset, skip_tran_Gx);

							computeTerminalVlw(block_states[i + j].types_C[k], block_states[i + j].signs_C[k],
								(k * F_step) + C_offset, skip_tran_Gx);
							symbol_translator.translate(block_states[i + j].types_C[k]);
							addVlwToCollection(block_states[i + j].types_C[k]);

							block_states[i + j].tran_H[k] = dense_vlw_t{ 0, 0 };
							for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
								bool skip_Hx = accumulateTranVlw(block_states[i + j].tran_H[k], (k * F_step) + Hx_offset + l, skip_tran_Hx);
								computeTerminalVlw(block_states[i + j].types_H[k][l], block_states[i + j].signs_H[k][l],
									(k * F_step) + Hx_offset + l, skip_Hx);
								symbol_translator.translate<decltype(symbol_translator)::types_H_codeparam>(
									block_states[i + j].types_H[k][l]);
								addVlwToCollection(block_states[i + j].types_H[k][l]);
							}
						}
					}

					symbol_translator.translate<decltype(symbol_translator)::tran_D_codeparam>(block_states[i + j].tran_D);
					addVlwToCollection(block_states[i + j].tran_D);
					symbol_translator.translate(block_states[i + j].tran_G);
					addVlwToCollection(block_states[i + j].tran_G);
					for (size_t k = 0; k < block_states[i + j].tran_H.size(); ++k) {
						symbol_translator.translate<decltype(symbol_translator)::tran_H_codeparam>(block_states[i + j].tran_H[k]);
						addVlwToCollection(block_states[i + j].tran_H[k]);
					}
				}

				{
					std::array<std::pair<size_t, ptrdiff_t>, vlwstreams.size()> optimal_code_options = { {
						{ vlwstreams[0]._size() * 2, -1 },
						{ vlwstreams[1]._size() * 3, -1 },
						{ vlwstreams[2]._size() * 4, -1 }
					} };

					auto update_optimal_code_option = [&](size_t index, size_t blen, size_t option) -> void {
						size_t current_bitsize = 0;
						while (vlwstreams[index]) {
							current_bitsize += entropy_translator._word_length(vlwstreams[index].get(), blen, option);
						}
						if (current_bitsize < optimal_code_options[index].first) {
							optimal_code_options[index] = { current_bitsize, 0 };
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
						gaggle_states[i >> 4].entropy_codeoptions[k + codeoptions_bias] = optimal_code_options[k].second;
						gaggle_states[i >> 4].code_marker_written[k + codeoptions_bias] = false;
						vlwstreams[k].reset();
					}
				}
			}
			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {}

			auto translateEncodeVlw = [&](dense_vlw_t& vlw) -> void {
				// TODO: warning, i is captured by reference
				size_t vlw_length = vlw.length;
				ptrdiff_t codeoption = gaggle_states[i >> 4].entropy_codeoptions[vlw_length];
				if (!gaggle_states[i >> 4].code_marker_written[vlw_length]) {
					// for word length 0 and 1 predicate is always false, should
					// never be executed for these lengths
					gaggle_states[i >> 4].code_marker_written[vlw_length] = true;
					size_t codeoption_bitsize = std::bit_width((size_t)(vlw_length - 1));
					output << vlw_t{ codeoption_bitsize, (vlw_t::type)(codeoption) };
				}
				output << entropy_translator.translate(vlw, codeoption);
			};

			// The code below write the computed words to the output stream. Per 4.5.3.1.7 and 
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
			i = 0;
			for (; i < input_size_truncated; i += items_per_gaggle) {
				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
					translateEncodeVlw(block_states[i + j].types_P);
					output << block_states[i + j].signs_P;
				}
			}
			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {}

			// stage 2
			i = 0;
			for (; i < input_size_truncated; i += items_per_gaggle) {
				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
					output << block_states[i + j].tran_B;
					translateEncodeVlw(block_states[i + j].tran_D);
					for (ptrdiff_t k = 0; k < F_num; ++k) {
						translateEncodeVlw(block_states[i + j].types_C[k]);
						output << block_states[i + j].signs_C[k];
					}
				}
			}
			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {}

			// stage 3
			i = 0;
			for (; i < input_size_truncated; i += items_per_gaggle) {
				for (ptrdiff_t j = 0; j < items_per_gaggle; ++j) {
					translateEncodeVlw(block_states[i + j].tran_G);
					for (ptrdiff_t k = 0; k < F_num; ++k) {
						translateEncodeVlw(block_states[i + j].tran_H[k]);
						for (ptrdiff_t l = 0; l < HxpF_num; ++l) {
							translateEncodeVlw(block_states[i + j].types_H[k][l]);
							output << block_states[i + j].signs_H[k][l];
						}
					}
				}
			}
			for (ptrdiff_t j = 0; j < last_gaggle_size; ++j) {}

			// stage 4
			for (ptrdiff_t j = 0; j < input.size; ++j) {
				output << combine_bword(current_bplane[j], bplane_mask[j]);
				bplane_mask[j] |= block_states[j].bplane_mask_transit;
				block_states[j].bplane_mask_transit = 0;
			}
		}
		{
			// fast forward to the next bplane, will make the container empty
			// if the current bplane was the last extracted.
			// Since the elements at the beginning only are erased, no moves/copies
			// occur and no iteraters are invalidated (as guaranteed by std::deque)

			// DEBUG NOTE: Fro some reason the whole container is zeroed (but size is correct)
			//
			current_bplane.erase(current_bplane.begin(), current_bplane.begin() + input.size);
		}
	}
	bool next_bplane = false;
	do {
		next_bplane = (b != 0);

		--b;	// formally UB
	} while (next_bplane);
}

template <typename T>
inline vlw_t combine_bword(T raw, T select_mask) {
	// UB when mask is all one bits 0xfffffff... due to shift equal to 
	// underlying type bitsize
	// 
	// TODO: implement contraints / nesessary casts
	// requires select_mask to be unsigned type

	// return a simple type that is represented by 2 full width
	// gprs, caller then casts/narrows the return value as needed
	size_t code{ 0 };
	size_t length{ 0 };
	size_t mask = -1;
	while (select_mask != 0) {
		size_t skip_count = std::countr_zero(select_mask);
		select_mask >>= skip_count;
		raw >>= skip_count;

		size_t select_count = std::countr_one(select_mask);
		code |= ((raw & (~(mask << select_count))) << length);

		length += select_count;
		raw >>= select_count;
		select_mask >>= select_count;
	}
	return { length, code };
}

#include <numeric>

template <typename T, typename obwT, size_t alignment>
void bplane(T* data, size_t size, size_t b, obitwrapper<obwT>& ostream) {
	std::array<std::make_unsigned_t<T>, alignment> masks;
	std::fill(masks.begin(), masks.end(), (0x01 << b));

	std::array<T, alignment> rshifts;
	std::array<T, alignment> lshifts;
	std::array<std::make_unsigned_t<T>, alignment> buffer;
	for (ptrdiff_t i = 0; i < rshifts.size(); ++i) {
		std::make_signed_t<T> relative_shift = ((std::make_signed_t<decltype(b)>)(b)) - alignment + 1 + i;
		rshifts[i] = relu(relative_shift);
		lshifts[i] = relu(-relative_shift);
	}

	std::make_unsigned_t<sufficient_integral_i<(alignment >> 3)>> serial_buffer;

	for (ptrdiff_t i = 0; i < size; i += alignment) {
		serial_buffer = 0;
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			buffer[j] = (data[i + j] & masks[j]);
		}
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			// TODO: check if promotion needed to integral sufficient to hold shifted value
			// TODO: compile-time checks to prevent the following
			// possible data loss if alignment > (sizeof(T) << 3)
			serial_buffer |= (buffer[j] << lshifts[j]) >> rshifts[j];
		}
		ostream << vlw_t{ alignment, serial_buffer };
	}
}

template <typename T, typename obwT, size_t alignment>
void bplanev4(T* data, size_t size, size_t belder, std::array<std::reference_wrapper<obitwrapper<obwT>>, 4> ostreams) {
	size_t b = relu(((std::make_signed_t<decltype(belder)>)(belder)) - ostreams.size());
	std::make_unsigned_t<T> mask = ((T)(0x01) << b);

	std::array<T, ostreams.size()> indicies{ 0 };
	size_t elder_shift = std::min(belder, ostreams.size() - 1);
	//  & (i < indicies.size()) is redandunt
	for (ptrdiff_t i = 0; (elder_shift > 0) & (i < indicies.size()); ++i) {
		indicies[i] = elder_shift;
		--elder_shift;
	}

	std::array<T, alignment> rshifts;
	std::array<T, alignment> lshifts;
	std::array<std::make_unsigned_t<T>, alignment> buffer;
	for (ptrdiff_t i = 0; i < rshifts.size(); ++i) {
		std::make_signed_t<T> relative_shift = ((std::make_signed_t<decltype(b)>)(b)) - alignment + 1 + i;
		rshifts[i] = relu(relative_shift);
		lshifts[i] = relu(-relative_shift);
	}

	std::array<std::make_unsigned_t<sufficient_integral_i<(alignment >> 3)>>, ostreams.size()> serial_buffers;

	for (ptrdiff_t i = 0; i < size; i += alignment) {
		for (ptrdiff_t k = 0; k < serial_buffers.size(); ++k) {
			serial_buffers[k] = 0;
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				buffer[j] = (data[i + j] & (mask << indicies[k]));
			}
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				// TODO: check if promotion needed to integral sufficient to hold shifted value
				// TODO: compile-time checks to prevent the following
				// possible data loss if alignment > (sizeof(T) << 3)
				serial_buffers[k] |= (buffer[j] << lshifts[j]) >> (rshifts[j] + indicies[k]);
			}
		}
		for (ptrdiff_t k = 0; k < serial_buffers.size(); ++k) {
			ostreams[k].get() << vlw_t{ alignment, serial_buffers[k] };
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

constexpr size_t block_index_to_buf_index(size_t l) {
	size_t bound_i = std::bit_width(l);
	bound_i += bound_i & 0x01;
	ptrdiff_t level = bound_i >> 1;
	ptrdiff_t mask = (0b11 << bound_i) >> 2;
	ptrdiff_t disp = (l & mask) >> (relu(level - 1) << 1);
	
	return (3 * relu(level - 1) + disp);
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
// In general, tran_B == '1' means that there is the first AC in joint descendals set to be 
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
// tran_D indicates what descendals set has the first AC coefficient to be selected in the current
// bitplane. While tran_B equals '0', all AC in all descendals set in the current bitplane are 
// nesessarily zeroes, therefore tran_D is not encoded when tran_B equals '0'. When tran_B is '1', 
// one bit in tran_D necessarily has value '1', and tran_D has length 3 for that bitplane only; 
// for the next bitplane, the length of tran_D is atmost 2; the length is decreased by the number of 
// '1' bits in tran_D for the previous bitplane.
// Typical transition of length for tran_D:
// [0 ->] 3 -> [2 -> [1 ->]] 0
// In general, bit '1' in tran_D means that there is the first AC in the corresponding descendals set 
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
// monotonously: it is possible that the length of tran_G can be increased in AC coefficient 
// type transition scenario that results in type -1 (0 -> 1 -> 2 -> -1); in such case 
// t_max(Gi, b) is defined by the type of another AC coefficient in Gi and can be evaluated to 
// 0 or 1, meaning the length of tran_G is increased by 1 for each Gi group that satisfy this 
// scenario (note that no other coefficient in Gi should have type 2 for this scenario to work, 
// otherwise the length of tran_G remains the same (or decreased due to other factors)).
// tran_G provides hint on whether setting of corresponding '1' bit in tran_D is caused by the
// corresponding grandchildren set coefficients. In addition, it indicates if it is needed to 
// track an analyze tran_H words.
// State transitions for tran_G are complex. Below is a scheme for possible transitions of 
// length of tran_G:
// +-> [0 ->] ([3 ->] | [2 ->] | [1 ->]) +
// |            +-<---<--+        |      |
// |            +-<---<--+-<---<--+      |
// +<---<---<---<---<---<---<---<---<---<+
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