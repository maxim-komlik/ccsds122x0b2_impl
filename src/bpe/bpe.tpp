#pragma once

#include <array>
#include <cstddef>

#include "core_types.hpp"

#include "obitwrapper.tpp"
#include "ibitwrapper.tpp"
#include "symbolbuffer.hpp"
#include "symbol_translate.hpp"
#include "entropy_translate.hpp"

#include "constant.hpp"

// TODO: why class template? has no data members dependent on template type parameter...
// is it related to the need of explicit instantiation for library exports?
template <typename T, size_t alignment = 16>
class BitPlaneEncoder: 
		// private constants::scale, // TODO: and not sure if private inheritance for block and gaggle is appropriate here
		private constants::block, 
		private constants::gaggle, 
		private constants::bpe, 
		private constants::k, 
		private constants::symbol,
		private constants::entropy {
	// inherently not copyable nor movable
	SymbolForwardTranslator symbol_fwd_translator;
	EntropyTranslator entropy_translator;
	
	bool use_heuristic_DC = false;
	bool use_heuristic_bdepthAc = false;

	struct {
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

		static constexpr size_t buffers_index_bias = 2;

		// well, these buffers are not truly a class property, it is used in 2 lambdas only
		// that are called consequently with similar parameters in the same parts of code inside 
		// encodeBpeStages. Moved to class definition to avoid memory reallocations only.
		// But these are temporal buffers by nature anyway.
		// 
		// TODO: may want to specify thread local storage for this entire object
		std::array<symbolbuffer, coded_vlw_lengths_count> content {
			symbolbuffer(256, 2),
			symbolbuffer(256, 3),
			symbolbuffer(288, 4)
		};

		inline symbolbuffer& operator[](ptrdiff_t length) {
			return this->content[length - this->buffers_index_bias];
		};

		inline void append_vlw(dense_vlw_t& vlw) {
			if (vlw.length >= min_coded_vlw_length) {
				(*this)[vlw.length].append(vlw.value);
			}
		};
	} entropy_buffers;

public:
	template <typename obwT>
	void encode(segment<T>& input, obitwrapper<obwT>& output);

	void set_use_heuristic_DC(bool value);
	bool get_use_heuristic_DC() const;
	void set_use_heuristic_bdepthAc(bool value);
	bool get_use_heuristic_bdepthAc() const;

	// TODO: implement early truncation
	void set_stop_after_DC(bool value);
	void set_stop_at_bplane(size_t bplane_index, size_t stage_index /* = 0b11 == stage_4 */);
private:
	template <typename D, typename obwT>
	void kOptimal(kParams<D> params, obitwrapper<obwT>& output_stream);

	template <typename D, typename obwT>
	void kHeuristic(kParams<D> params, obitwrapper<obwT>& output_stream);

	template <typename D, typename obwT>
	void kEncode(kParams<D> params, bool use_heuristic, obitwrapper<obwT>& output_stream);

	template <typename obwT>
	void encodeBpeStages(segment<T>& input, obitwrapper<obwT>& output);
};

template <typename T>
class BitPlaneDecoder: 
	// private constants::scale, // TODO: and not sure if private inheritance for block and gaggle is appropriate here
		private constants::block, 
		private constants::gaggle, 
		private constants::bpe, 
		private constants::k, 
		private constants::symbol,
		private constants::entropy {
	SymbolBackwardTranslator symbol_bwd_translator;
	EntropyTranslator entropy_translator;

public:
	template <typename ibwT>
	void decode(segment<T>& output, ibitwrapper<ibwT>& input);

	// TODO: implement early truncation
	void set_stop_after_DC(bool value);
	void set_stop_at_bplane(size_t bplane_index, size_t stage_index /* = 0b11 == stage_4 */);
private:
	template <typename D, typename ibwT>
	void kReverse(kParams<D>& params, ibitwrapper<ibwT>& input_stream);

	template <typename D, typename ibwT>
	void kDecode(kParams<D>& params, ibitwrapper<ibwT>& input_stream);

	template <typename ibwT>
	void decodeBpeStages(segment<T>& output, ibitwrapper<ibwT>& input);
};
