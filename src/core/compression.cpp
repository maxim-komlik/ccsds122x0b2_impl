#include "compression.hpp"

namespace per_thread {

	// private interface
	namespace {
		template <typename T>
		struct compression_resources {
			using dwtT = compressors<T>::dwtT;
			using bpeT = compressors<T>::bpeT;
			std::unique_ptr<ForwardWaveletTransformer<dwtT>> transformer = nullptr;
			std::unique_ptr<SegmentAssembler<dwtT>> assmebler = nullptr;
			std::unique_ptr<BitPlaneEncoder<bpeT>> encoder = nullptr;
		};

		template <typename T>
		compression_resources<T>& get_compression_resources(const session_context& context) {
			thread_local compression_resources<T> compr_resources;
			return compr_resources;
		}


		template <typename T>
		struct decompression_resources {
			using dwtT = decompressors<T>::dwtT;
			using bpeT = decompressors<T>::bpeT;
			std::unique_ptr<BackwardWaveletTransformer<dwtT>> transformer = nullptr;
			std::unique_ptr<SegmentDisassembler<dwtT>> disassmebler = nullptr;
			std::unique_ptr<BitPlaneDecoder<bpeT>> decoder = nullptr;
		};

		template <typename T>
		decompression_resources<T>& get_decompression_resources(const session_context& context) {
			thread_local decompression_resources<T> compr_resources;
			return compr_resources;
		}
	}

	template <typename T> 
	ForwardWaveletTransformer<typename compressors<T>::dwtT>& compressors<T>::get_transformer(const session_context& context) {
		auto& resources = get_compression_resources<T>(context);
		[[unlikely]]
		if (resources.transformer == nullptr) {
			resources.transformer = std::make_unique<ForwardWaveletTransformer<dwtT>>();
		}
		return *(resources.transformer);
	}

	template <typename T> 
	SegmentAssembler<typename compressors<T>::dwtT>& compressors<T>::get_assembler(const session_context& context) {
		auto& resources = get_compression_resources<T>(context);
		[[unlikely]]
		if (resources.assmebler == nullptr) {
			resources.assmebler = std::make_unique<SegmentAssembler<dwtT>>();
		}
		return *(resources.assmebler);
	}

	template <typename T>
	BitPlaneEncoder<typename compressors<T>::bpeT>& compressors<T>::get_encoder(const session_context& context) { 
		auto& resources = get_compression_resources<T>(context);
		[[unlikely]]
		if (resources.encoder == nullptr) {
			resources.encoder = std::make_unique<BitPlaneEncoder<bpeT>>();
		}
		return *(resources.encoder);
	}

	template <typename T>
	void compressors<T>::free_compression_resources(const session_context& context) {
		auto& resources = get_compression_resources<T>(context);
		resources.transformer.reset(nullptr);
		resources.assmebler.reset(nullptr);
		resources.encoder.reset(nullptr);
	}



	template <typename T>
	BackwardWaveletTransformer<typename decompressors<T>::dwtT>& decompressors<T>::get_transformer(const session_context& context) {
		auto& resources = get_decompression_resources<T>(context);
		[[unlikely]]
		if (resources.transformer == nullptr) {
			resources.transformer = std::make_unique<BackwardWaveletTransformer<dwtT>>();
		}
		return *(resources.transformer);
	}

	template <typename T>
	SegmentDisassembler<typename decompressors<T>::dwtT>& decompressors<T>::get_disassembler(const session_context& context) {
		auto& resources = get_decompression_resources<T>(context);
		[[unlikely]]
		if (resources.disassmebler == nullptr) {
			resources.disassmebler = std::make_unique<SegmentDisassembler<dwtT>>();
		}
		return *(resources.disassmebler);
	}

	template <typename T>
	BitPlaneDecoder<typename decompressors<T>::bpeT>& decompressors<T>::get_decoder(const session_context& context) {
		auto& resources = get_decompression_resources<T>(context);
		[[unlikely]]
		if (resources.decoder == nullptr) {
			resources.decoder = std::make_unique<BitPlaneDecoder<bpeT>>();
		}
		return *(resources.decoder);
	}

	template <typename T>
	void decompressors<T>::free_decompression_resources(const session_context& context) {
		auto& resources = get_decompression_resources<T>(context);
		resources.transformer.reset(nullptr);
		resources.disassmebler.reset(nullptr);
		resources.decoder.reset(nullptr);
	}
}

//
// explicit instantiation section

template struct per_thread::compressors<int8_t>;
template struct per_thread::compressors<int16_t>;
template struct per_thread::compressors<int32_t>;
template struct per_thread::compressors<int64_t>;

template struct per_thread::compressors<float>;
template struct per_thread::compressors<double>;
// template struct per_thread::compressors<long double>;


template struct per_thread::decompressors<int8_t>;
template struct per_thread::decompressors<int16_t>;
template struct per_thread::decompressors<int32_t>;
template struct per_thread::decompressors<int64_t>;

template struct per_thread::decompressors<float>;
template struct per_thread::decompressors<double>;
// template struct per_thread::decompressors<long double>;
