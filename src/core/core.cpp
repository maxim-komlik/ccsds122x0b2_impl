#include <memory>

#include "io/io_settings.hpp"
#include "io/io_contexts.hpp"
#include "io/session_context.hpp"
#include "io/file_sink.hpp"
#include "io/tasking.hpp"

#include "dwt/dwt.hpp"
#include "dwt/segment_assembly.hpp"

#include "bpe/bpe.tpp"

template <typename T>
std::unique_ptr<sink<T>> make_sink(sink_type type);

template <typename T>
std::unique_ptr<BitPlaneEncoder<T>> allocate_encoder(const session_context& context);

template <typename T>
std::unique_ptr<ForwardWaveletTransformer<T>> make_transformer();

template <typename T>
std::unique_ptr<SegmentAssembler<T>> make_assembler();

namespace per_thread {
	// public interface
	template <typename T>
	BitPlaneEncoder<T>& get_encoder(const session_context& context) { 
		auto& resources = get_compression_resources<T>(context);
		if (resources.encoder == nullptr) {
			resources.encoder = std::make_unique<BitPlaneEncoder<T>>();
		}
		return resources.encoder;
	}

	template <typename T> 
	ForwardWaveletTransformer<T>& get_transformer(const session_context& context) {
		auto& resources = get_compression_resources<T>(context);
		if (resources.transformer == nullptr) {
			resources.transformer = std::make_unique<ForwardWaveletTransformer<T>>();
		}
		return resources.transformer;
	}

	template <typename T> 
	SegmentAssembler<T>& get_assember(const session_context& context) {
		auto& resources = get_compression_resources<T>(context);
		if (resources.assmebler == nullptr) {
			resources.assmebler = std::make_unique<SegmentAssembler<T>>();
		}
		return resources.assmebler;
	}

	// private interface
	namespace {
		template <typename T>
		struct compression_resources {
			std::unique_ptr<ForwardWaveletTransformer<T>> transformer = nullptr;
			std::unique_ptr<SegmentAssembler<T>> assmebler = nullptr;
			std::unique_ptr<BitPlaneEncoder<T>> encoder = nullptr;
		};

		template <typename T>
		compression_resources<T>& get_compression_resources(const session_context& context) {
			thread_local compression_resources compr_resources;
			return compr_resources;
		}

		void free_compression_resources(const session_context& context) {
			// but has to be template to access proper set of resources
		}
	}
}

size_t generate_session_id();
size_t generate_compression_id();

void do_regular_compression_sig_001(
	const std::shared_ptr<session_context> context, 
	// but it is expected that lifetime of the session context is guaranteed to 
	// enclose the lifetime of thread and segment data
	std::unique_ptr<compression_context<uint64_t>> data);
// but compression context is really owned by the session and dependencies 
// between compression contexts are controlled by the session
// 
// but session may own some token instead of complete data, it does not need 
// segment data (but sink properties may be queried for encoding control)
//

void do_regular_compression(const session_context& context, compression_context<uint64_t>& data) {
	std::unique_ptr<BitPlaneEncoder<uint64_t>> encoder = allocate_encoder<uint64_t>(context);
	auto [param_compr, flag_compr] = get_compression_settings(context, data.id);
	auto [param_seg, flag_seg] = get_segment_settings(context, data.id);

	encoder->set_use_heuristic_DC(param_seg.heuristic_quant_DC);
	encoder->set_use_heuristic_bdepthAc(param_seg.heiristic_bdepth_AC);
	if (param_compr.early_termination) {
		encoder->set_stop_after_DC(param_compr.DC_stop);
		encoder->set_stop_at_bplane(param_compr.bplane_stop, param_compr.stage_stop);
	} else {
		// refactor default params
		encoder->set_stop_after_DC(false);
		encoder->set_stop_at_bplane(0, 0b11);
	}

	data.dst->setup_session(param_seg, param_compr, flag_seg, flag_compr);
	data.dst->init_session();
	data.dst->process_segment(*encoder, *(data.segment_data));
	data.dst->finish_session();

	// TODO: register completed task via context.
}

// void general_flow() {
void general_flow(session_settings params_session, segment_settings params_segment, compression_settings params_compression) {

	
	// input parameters:
	std::vector<bitmap<int64_t>> img_multichannel;

	// user-provided options and metadata:
	// session_settings params_session;
	// segment_settings params_segment;
	// compression_settings params_compression;

	sink_type dst_type = sink_type::memory;
	std::string dst_desc = "";

	// body:
	auto cx_session = std::make_shared<session_context>();
	cx_session->id = generate_session_id();
	cx_session->settings_session = params_session;
	cx_session->dst_type = dst_type;
	cx_session->compr_settings.push_back(std::make_pair(0, params_compression));
	cx_session->seg_settings.push_back(std::make_pair(0, params_segment));

	// maybe add bitmap wrapper for multichannel images that hides 
	// std::vector<std::unique_ptr<bitmap>>> details? We need intermediate pointers
	// to weaken memory dependencies to boost parallel channel processing
	cx_session->image_channels = std::move(img_multichannel);
	// validate image here, make sure dimensions meet dwt requirements. Pad image if necessary

	// allocate session context on heap. Create shared_ptr and use it to initialize
	// split consumers.
	// 
	// split by image channels
	// check session sink type to adjust logic

	// split

	// Here starts the first encoding stage: dwt+precoder.
	// gets session context ptr and subject bitmap reference as input parameters
	// 
	// This stage is expected to be heavy in computational and memory usage terms
	//
	auto dwt_input = cx_session->image_channels[0];
	if (cx_session->settings_session.transpose) {
		dwt_input = dwt_input.transpose();
	}
	auto transformer = make_transformer<int64_t>(); // TODO: make bitmap type constraints for signed integers only
	transformer->get_scale().set_shifts(cx_session->settings_session.shifts);
	auto dwt_result = transformer->apply(dwt_input);
	// here the need to refactor construction of dwt and segment_assembly arises
	auto assembler = make_assembler<int64_t>();
	// assembler->set_segment_size(get_segment_settings(cx_session, 0).first.size);
	assembler->set_shifts(transformer->get_scale().get_shifts());
	auto segments = assembler->apply(std::move(dwt_result));
	// here finishes first encoding stage: dwt+precoder.
	// 
	// output is collection of precoded segments (managed by unique_ptr)
	// heavy memory allocations and computations are completed, pass result to 
	// the next task to dispatch segments and allocate sink resources...

	// then
	auto compr_manager_input = std::move(segments);
	size_t seg_num_limit = cx_session->compression_contexts.size() + compr_manager_input.size();
	while (cx_session->compression_contexts.size() < 256) {
		auto seg_sink = make_sink<uint64_t>(cx_session->dst_type);
		compression_context<uint64_t> compr_cx {

		};
	}
}
