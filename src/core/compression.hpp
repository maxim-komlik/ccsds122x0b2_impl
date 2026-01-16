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

}

extern template struct per_thread::compressors<int8_t>;
extern template struct per_thread::compressors<int16_t>;
extern template struct per_thread::compressors<int32_t>;
extern template struct per_thread::compressors<int64_t>;

// template <typename T>
// std::unique_ptr<BitPlaneEncoder<T>> make_encoder(const session_context& context);
// 
// template <typename T>
// std::unique_ptr<ForwardWaveletTransformer<T>> make_transformer();
// 
// template <typename T>
// std::unique_ptr<SegmentAssembler<T>> make_assembler();
