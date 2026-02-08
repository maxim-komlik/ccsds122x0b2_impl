#pragma once

#include <memory>

#include "compressor_types.hpp"
#include "io/session_context.hpp"
#include "io/sink.hpp"

#include "dwt/dwt.hpp"
#include "dwt/segment_assembly.hpp"
#include "bpe/bpe.tpp"

namespace per_thread {

	template <typename T>
	struct compressors {
		using types = compressor_type_params<T>;
		using dwt_data_type = compressor_type_params<T>::dwt_type;
		using bitplane_data_type = compressor_type_params<T>::segment_type;

		using dwtT = dwt_data_type;
		using bpeT = bitplane_data_type;

		// public interface
		static ForwardWaveletTransformer<dwtT>& get_transformer(const session_context& context);
		static SegmentAssembler<dwtT>& get_assembler(const session_context& context);
		static BitPlaneEncoder<bpeT>& get_encoder(const session_context& context);

		static void free_compression_resources(const session_context& context);
	};

	template <typename T>
	struct decompressors {
		using types = compressor_type_params<T>;

		using dwtT = types::dwt_type;
		using bpeT = types::segment_type;

		// public interface
		static BackwardWaveletTransformer<dwtT>& get_transformer(const session_context& context);
		static SegmentDisassembler<dwtT>& get_disassembler(const session_context& context);
		static BitPlaneDecoder<bpeT>& get_decoder(const session_context& context);

		static void free_decompression_resources(const session_context& context);
	};

}

//
// explicit instantiation section

extern template struct per_thread::compressors<int8_t>;
extern template struct per_thread::compressors<int16_t>;
extern template struct per_thread::compressors<int32_t>;
extern template struct per_thread::compressors<int64_t>;

extern template struct per_thread::compressors<float>;
extern template struct per_thread::compressors<double>;
extern template struct per_thread::compressors<long double>;

extern template struct per_thread::decompressors<int8_t>;
extern template struct per_thread::decompressors<int16_t>;
extern template struct per_thread::decompressors<int32_t>;
extern template struct per_thread::decompressors<int64_t>;

extern template struct per_thread::decompressors<float>;
extern template struct per_thread::decompressors<double>;
extern template struct per_thread::decompressors<long double>;
