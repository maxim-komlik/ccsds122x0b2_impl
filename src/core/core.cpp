#include <vector>
#include <memory>
#include <algorithm>


#include "io/io_settings.hpp"
#include "io/io_contexts.hpp"
#include "io/session_context.hpp"
#include "io/tasking.hpp"

#include "io.hpp"
#include "samples/routines.tpp"
// 
// // TODO: useful functions to export internal functionality of modules:
// //	image dimensions padding adjustment
// //
// 
// template <typename T>
// size_t compute_image_bdepth(const bitmap<T>& src) {
// 	size_t result = 0;
// 	for (ptrdiff_t i = 0; i < src.get_meta().height; ++i) {
// 		bitmap_row row = src[i];
// 		result = std::max(result, bdepthv<T, 16>(row.ptr(), row.width()));
// 	}
// 	return result;
// }
// 
// // #include <iostream>
// // 
// // template <typename T>
// // struct another_test_struct {
// // 	T value;
// // };
// // 
// // template <typename T>
// // struct test_struct {
// // 	T value;
// // 	another_test_struct<float> another_value;
// // 	std::string str_value;
// // 	std::vector<char> vector_value;
// // };
// // 
// // struct move_only_type {
// // 	move_only_type() {}
// // 
// // 	move_only_type(move_only_type&& other) = default;
// // 	move_only_type& operator=(move_only_type&& other) = default;
// // };
// 
// void compress_image(std::vector<bitmap<int32_t>>& img) {
// 	auto& first_channel = img[0];
// 	size_t channel_num = img.size();
// 
// 	session_settings ssettings;
// 	ssettings.dwt_type = dwt_type_t::idwt;
// 	ssettings.img_width = first_channel.get_meta().width;
// 	ssettings.pixel_bdepth = std::max(
// 		{ 
// 			compute_image_bdepth(img[0]),
// 			compute_image_bdepth(img[1]),
// 			compute_image_bdepth(img[2])
// 		});
// 	ssettings.signed_pixel = false;
// 	ssettings.transpose = false;
// 	// TODO: padding computation utility/design
// 	ssettings.rows_pad_count = ((first_channel.get_meta().height + 7) & (~7)) - first_channel.get_meta().height;
// 	ssettings.codeword_size = sizeof(uintptr_t);
// 	ssettings.custom_shifts = false;
// 	ssettings.shifts = { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };
// 
// 	compression_settings csettings;
// 	csettings.seg_byte_limit = 2 << 16;
// 	csettings.use_fill = false;
// 	csettings.early_termination = false;
// 	csettings.DC_stop = false;
// 	csettings.bplane_stop = 0;
// 	csettings.stage_stop = static_cast<size_t>(bpe_stage_index_t::stage_4);
// 
// 	segment_settings segsettings;
// 	segsettings.heuristic_quant_DC = false;
// 	segsettings.heuristic_bdepth_AC = false;
// 	segsettings.size = 1200;
// 
// 
// 	auto cx_session = std::make_shared<session_context>(std::move(img), sink_type::file);
// 	cx_session->id = generate_session_id();
// 	cx_session->settings_session = ssettings;
// 	cx_session->compr_settings.push_back(std::make_pair(0, csettings));
// 	cx_session->seg_settings.push_back(std::make_pair(0, segsettings));	
// 
// 	using routine_set = compression_routines<int32_t>;
// 	static_assert(sizeof(routine_set::types::block_type) == sizeof(routine_set::types::dwt_type));
// 	static_assert(sizeof(routine_set::types::segment_type) == sizeof(routine_set::types::dwt_type));
// 	static_assert(sizeof(routine_set::types::subband_type) == sizeof(routine_set::types::dwt_type));
// 	static_assert(sizeof(routine_set::types::bpe_type) == sizeof(routine_set::types::dwt_type));
// 	// cx_session->compr_data = session_context::compression_data<routine_set::types::dwt_type>{};
// 
// 
// 	using transform_tree = task_pool::FlowGraph::root::then<decltype([](dwt_context cx) {
// 			return routine_set::preprocess_image<int32_t>(std::move(cx));
// 		})>::split<decltype([](dwt_context cx) {
// 			routine_set::transform_fragment<int32_t>(std::move(cx));
// 		})>;
// 
// 	using compress_tree = task_pool::FlowGraph::root::then<decltype([](dwt_context cx) -> segmentation_context<routine_set::subband_type> {
// 			return routine_set::preprocess_fragments(std::move(cx));
// 		})>::then<decltype([](segmentation_context<routine_set::subband_type> cx) -> std::vector<compression_context<routine_set::segment_type>> {
// 			return routine_set::assemble_segments(std::move(cx));
// 		})>::split<decltype([](compression_context<routine_set::segment_type> cx) -> void {
// 			routine_set::compress_segment(std::move(cx));
// 		})>;
// 
// 	std::vector<dwt_context> transform_input_contexts;
// 	std::vector<dwt_context> compress_input_contexts;
// 
// 	img_pos input_frame = first_channel.single_frame_params();
// 	for (ptrdiff_t z = 0; z < channel_num; ++z) {
// 		input_frame.z = z;
// 		dwt_context transform_context {
// 			generate_dwt_id(),
// 			cx_session->channel_contexts[z],
// 			input_frame
// 		};
// 
// 		dwt_context compress_context {
// 			generate_dwt_id(),
// 			cx_session->channel_contexts[z],
// 			input_frame
// 		};
// 
// 		transform_input_contexts.push_back(transform_context);
// 		compress_input_contexts.push_back(compress_context);
// 	}
// 
// 	task_pool pool;
// 	for (ptrdiff_t z = 0; z < channel_num; ++z) {
// 		cx_session->channel_contexts[z].register_operation(transform_input_contexts[z]);
// 		pool.add_tasks(task_pool::FlowGraph::parse<transform_tree>(std::move(transform_input_contexts[z])));
// 	}
// 	pool.execute_flow();
// 
// 
// 	for (ptrdiff_t z = 0; z < channel_num; ++z) {
// 		cx_session->channel_contexts[z].register_operation(compress_input_contexts[z]);
// 		pool.add_tasks(task_pool::FlowGraph::parse<compress_tree>(std::move(compress_input_contexts[z])));
// 	}
// 	pool.execute_flow();
// }


// // void general_flow() {
// void general_flow(session_settings params_session, segment_settings params_segment, compression_settings params_compression) {
// 
// 	
// 	// input parameters:
// 	std::vector<bitmap<int64_t>> img_multichannel;
// 
// 	// user-provided options and metadata:
// 	// session_settings params_session;
// 	// segment_settings params_segment;
// 	// compression_settings params_compression;
// 
// 	sink_type dst_type = sink_type::memory;
// 	std::string dst_desc = "";
// 
// 	// body:
// 	auto cx_session = std::make_shared<session_context>();
// 	cx_session->id = generate_session_id();
// 	cx_session->settings_session = params_session;
// 	cx_session->dst_type = dst_type;
// 	cx_session->compr_settings.push_back(std::make_pair(0, params_compression));
// 	cx_session->seg_settings.push_back(std::make_pair(0, params_segment));
// 
// 	// maybe add bitmap wrapper for multichannel images that hides 
// 	// std::vector<std::unique_ptr<bitmap>>> details? We need intermediate pointers
// 	// to weaken memory dependencies to boost parallel channel processing
// 	cx_session->image_channels = std::move(img_multichannel);
// 	// validate image here, make sure dimensions meet dwt requirements. Pad image if necessary
// 
// 	// allocate session context on heap. Create shared_ptr and use it to initialize
// 	// split consumers.
// 	// 
// 	// split by image channels
// 	// check session sink type to adjust logic
// 
// 	// split
// 
// 	// Here starts the first encoding stage: dwt+precoder.
// 	// gets session context ptr and subject bitmap reference as input parameters
// 	// 
// 	// This stage is expected to be heavy in computational and memory usage terms
// 	//
// 	
// 	// auto transformer = make_transformer<int64_t>(); // TODO: make bitmap type constraints for signed integers only
// 	// here finishes first encoding stage: dwt+precoder.
// 	// 
// 	// output is collection of precoded segments (managed by unique_ptr)
// 	// heavy memory allocations and computations are completed, pass result to 
// 	// the next task to dispatch segments and allocate sink resources...
// 
// }
