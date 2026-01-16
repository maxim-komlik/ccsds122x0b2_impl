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
}

template struct per_thread::compressors<int8_t>;
template struct per_thread::compressors<int16_t>;
template struct per_thread::compressors<int32_t>;
template struct per_thread::compressors<int64_t>;
