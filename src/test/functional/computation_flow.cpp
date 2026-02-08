#include "gtest/gtest.h"

#include <array>
#include <tuple>
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <charconv>
#include <filesystem>

#include "utils.hpp"
#include "../test_utils.tpp"

#include "io/io_settings.hpp"
#include "io/io_contexts.hpp"
#include "io/session_context.hpp"
#include "io/tasking.hpp"
#include "core/samples/routines.tpp"

// TODO: useful functions to export internal functionality of modules:
//	image dimensions padding adjustment
//

template <typename T>
size_t compute_image_bdepth(const bitmap<T>& src) {
	size_t result = 0;
	for (ptrdiff_t i = 0; i < src.get_meta().height; ++i) {
		bitmap_row row = src[i];
		result = std::max(result, bdepthv<T, 16>(row.ptr(), row.width()));
	}
	return result;
}

size_t compress_image(std::vector<bitmap<int32_t>>&& img) {
	auto& first_channel = img[0];
	size_t channel_num = img.size();

	session_settings ssettings;
	ssettings.dwt_type = dwt_type_t::idwt;
	ssettings.img_width = first_channel.get_meta().width;
	ssettings.pixel_bdepth = 0;
	for (auto& item : img) {
		ssettings.pixel_bdepth = std::max(ssettings.pixel_bdepth, compute_image_bdepth(item));
	}
	// std::max({
	// 	compute_image_bdepth(img[0]),
	// 	compute_image_bdepth(img[1]),
	// 	compute_image_bdepth(img[2])
	// });
	ssettings.signed_pixel = true;
	ssettings.transpose = false;
	// TODO: padding computation utility/design
	ssettings.rows_pad_count = ((first_channel.get_meta().height + 7) & (~7)) - first_channel.get_meta().height;
	ssettings.codeword_size = sizeof(uintptr_t);
	ssettings.custom_shifts = false;
	ssettings.shifts = { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };

	compression_settings csettings;
	csettings.seg_byte_limit = 2 << 16;
	csettings.use_fill = false;
	csettings.early_termination = false;
	csettings.DC_stop = false;
	csettings.bplane_stop = 0;
	csettings.stage_stop = static_cast<size_t>(bpe_stage_index_t::stage_4);

	segment_settings segsettings;
	segsettings.heuristic_quant_DC = false;
	segsettings.heuristic_bdepth_AC = false;
	segsettings.size = 1200;

	io_data_registry registry(storage_type::file);

	auto cx_session = std::make_shared<session_context>(registry);
	cx_session->id = generate_session_id();
	cx_session->settings_session = ssettings;
	cx_session->compr_settings.push_back(std::make_pair(0, csettings));
	cx_session->seg_settings.push_back(std::make_pair(0, segsettings));

	using routine_set = compression_routines<int32_t>;
	static_assert(sizeof(routine_set::types::block_type) == sizeof(routine_set::types::dwt_type));
	static_assert(sizeof(routine_set::types::segment_type) == sizeof(routine_set::types::dwt_type));
	static_assert(sizeof(routine_set::types::subband_type) == sizeof(routine_set::types::dwt_type));
	static_assert(sizeof(routine_set::types::bpe_type) == sizeof(routine_set::types::dwt_type));
	// cx_session->compr_data = session_context::compression_data<routine_set::types::dwt_type>{};


	using transform_tree = task_pool::flow_graph::root::then<decltype([](dwt_context cx) {
		return routine_set::preprocess_image<int32_t>(std::move(cx));
	})>::split<decltype([](dwt_context cx) {
		routine_set::transform_fragment<int32_t>(std::move(cx));
	})>;

	using compress_tree = task_pool::flow_graph::root::then<decltype([](dwt_context cx) -> 
			segmentation_context<routine_set::subband_type, routine_set::segment_type> {
		return routine_set::preprocess_fragments(std::move(cx));
	})>::then<decltype([](segmentation_context<routine_set::subband_type, routine_set::segment_type> cx) -> 
			std::vector<compression_context<routine_set::segment_type>> {
		return routine_set::assemble_segments(std::move(cx));
	})>::split<decltype([](compression_context<routine_set::segment_type> cx) -> void {
		routine_set::compress_segment(std::move(cx));
	})>;

	std::vector<dwt_context> transform_input_contexts;
	std::vector<dwt_context> compress_input_contexts;

	img_pos input_frame = first_channel.single_frame_params();
	cx_session->init_channel_contexts<routine_set::types::dwt_type>(channel_num);
	for (ptrdiff_t z = 0; z < channel_num; ++z) {
		input_frame.z = z;
		const data_descriptor& descriptor = registry.put_input(
			image_memory_descriptor<int32_t>(std::move(img[z]), z));
		dwt_context transform_context{
			generate_dwt_id(),
			cx_session->channel_contexts[z],
			input_frame, 
			descriptor
		};

		dwt_context compress_context{
			generate_dwt_id(),
			cx_session->channel_contexts[z],
			input_frame, 
			descriptor
		};

		transform_input_contexts.push_back(transform_context);
		compress_input_contexts.push_back(compress_context);
	}

	task_pool pool;
	for (ptrdiff_t z = 0; z < channel_num; ++z) {
		cx_session->channel_contexts[z].descriptors.register_operation(transform_input_contexts[z]);
		pool.add_tasks(task_pool::flow_graph::parse<transform_tree>(std::move(transform_input_contexts[z])));
	}
	pool.execute_flow();


	for (ptrdiff_t z = 0; z < channel_num; ++z) {
		cx_session->channel_contexts[z].descriptors.register_operation(compress_input_contexts[z]);
		pool.add_tasks(task_pool::flow_graph::parse<compress_tree>(std::move(compress_input_contexts[z])));
	}
	pool.execute_flow();

	return cx_session->id;
}

std::u8string make_path_pattern(size_t session_id, size_t channel_id) {
	constexpr std::string_view segment_description = "segment"sv;
	constexpr std::string_view channel_description = "channel"sv;
	constexpr std::string_view separator = "_"sv;
	constexpr size_t name_max_size =
		std::numeric_limits<size_t>::digits10 +		// {session id}
		separator.size() +							// _
		channel_description.size() +				// channel
		separator.size() +							// _
		std::numeric_limits<size_t>::digits10 +		// {channel index}
		separator.size() +							// _
		segment_description.size() +				// segment
		separator.size() +							// _
		sizeof(char);								// {terminator}

	std::array<char, name_max_size> filename_buffer{ char{} };

	auto conversion_result = std::to_chars(filename_buffer.data(),
		filename_buffer.data() + filename_buffer.size(), session_id);
	if (conversion_result.ec != std::errc{}) {
		// C++23 nonreachable
	}

	auto pos = std::copy_n(separator.data(), separator.size(), conversion_result.ptr);
	pos = std::copy_n(channel_description.data(), channel_description.size(), pos);
	pos = std::copy_n(separator.data(), separator.size(), pos);

	conversion_result = std::to_chars(pos,
		filename_buffer.data() + filename_buffer.size(), channel_id);
	if (conversion_result.ec != std::errc{}) {
		// C++23 nonreachable
	}

	pos = std::copy_n(separator.data(), separator.size(), conversion_result.ptr);
	pos = std::copy_n(segment_description.data(), segment_description.size(), pos);
	pos = std::copy_n(separator.data(), separator.size(), pos);

	return std::u8string(filename_buffer.data(), pos);
}

template <typename ibwT, typename imgT, typename dwtT>
struct decompression_type_params {
	using ibw_t = ibwT;
	using img_t = imgT;
	using dwt_t = dwtT;

	static void decompress(std::shared_ptr<session_context> cx, 
			std::vector<std::reference_wrapper<const data_descriptor>>&& handles) {
		using routine_set = compression_routines<dwt_t>;
		using decompress_tree = task_pool::flow_graph::root::then<decltype([](
				compression_context<typename routine_set::segment_type> cx) -> void {
			routine_set::template decode_segment<ibw_t>(std::move(cx));
		})>;
		
		using transform_tree = task_pool::flow_graph::root::then<decltype([](
				segmentation_context<typename routine_set::subband_type, typename routine_set::segment_type> cx) {
			return routine_set::disassemble_segments(std::move(cx));
		})>::split<decltype([](segmentation_context<routine_set::subband_type, routine_set::segment_type> cx) -> void {
			routine_set::template restore_image<img_t>(std::move(cx));
		})>;

		using postprocess_tree = task_pool::flow_graph::root::then<decltype([](
				segmentation_context<routine_set::subband_type, routine_set::segment_type> cx) -> void {
			routine_set::template postprocess_image<img_t>(std::move(cx));
		})>;

		task_pool pool;
		using decompress_task_t = task_pool::flow_graph::parse<decompress_tree>;
		for (ptrdiff_t id = 0; id < handles.size(); ++id) {
			size_t z = handles[id].get().get_exported_data().channel_id.value();
			compression_context<typename routine_set::segment_type> context{
				generate_compression_id(),
				cx->channel_contexts[z],
				std::make_unique<segment<routine_set::segment_type>>(),
				handles[id]
			};
			context.segment_data->id = id;
			cx->channel_contexts[z].descriptors.register_operation(context);
			pool.add_tasks(decompress_task_t(std::move(context))); // TODO: this way to populate pool queue 
				// is expensive...
		}
		pool.execute_flow();

		for (ptrdiff_t i = 0; i < cx->channel_contexts.size(); ++i) {
			segmentation_context<routine_set::subband_type, routine_set::segment_type> context{
				generate_segmentation_id(), 
				cx->channel_contexts[i], 
				{}, 
				nullptr,
				{0}
			};
			cx->channel_contexts[i].descriptors.register_operation(context);
			pool.add_tasks(task_pool::flow_graph::parse<transform_tree>(std::move(context)));
		}
		pool.execute_flow();

		for (ptrdiff_t i = 0; i < cx->channel_contexts.size(); ++i) {
			segmentation_context<routine_set::subband_type, routine_set::segment_type> context{
				generate_segmentation_id(),
				cx->channel_contexts[i],
				{},
				nullptr,
				{0}
			};

			// dwt_context consumer_cx{
			// 	generate_dwt_id(), 
			// 	cx.channel_cx, 
			// 	cx.frame, 
			// 	descriptor
			// };

			cx->channel_contexts[i].descriptors.register_operation(context);
			pool.add_tasks(task_pool::flow_graph::parse<postprocess_tree>(std::move(context)));
		}
		pool.execute_flow();
	}
};

std::vector<bitmap<int32_t>> restore_image(size_t session_id, size_t channel_id) {
	std::vector<std::filesystem::path> segment_paths;
	std::u8string segment_filename_pattern = make_path_pattern(session_id, channel_id);
	std::filesystem::directory_iterator it(std::filesystem::current_path(), 
		std::filesystem::directory_options::skip_permission_denied);
	while (it != std::filesystem::directory_iterator{}) {
		if (it->path().filename().u8string().starts_with(segment_filename_pattern)) {
			bool valid = true;
			valid &= it->exists();
			valid &= it->is_regular_file();
			if (!valid) {
				// oops?
			}
			segment_paths.push_back(it->path());
		}
		++it;
	}

	io_data_registry io_registry(storage_type::file);

	std::vector<std::pair<size_t, std::reference_wrapper<const data_descriptor>>> handles_unordered;
	for (auto& item : segment_paths) {
		size_t segment_id = 0;
		std::string segment_filename = item.filename().string();
		auto conversion_result = std::from_chars(segment_filename.data() + segment_filename_pattern.size(), 
			segment_filename.data() + segment_filename.size(), segment_id);
		
		handles_unordered.emplace_back(
			segment_id, 
			io_registry.put_input(segment_file_descriptor(item, channel_id, segment_id)));
	}

	std::sort(handles_unordered.begin(), handles_unordered.end(),
		[](const auto& lhs, const auto& rhs) -> bool {
			return lhs.first < rhs.first;
		});

	size_t next_segment_id = handles_unordered.front().first;

	bool valid = true;
	valid &= (next_segment_id == 0);
	std::for_each(handles_unordered.cbegin(), handles_unordered.cend(),
		[&next_segment_id, &valid](const auto& item) -> void {
			valid &= (item.first == next_segment_id);
			++next_segment_id;
		});

	if (!valid) {
		// input segment sequence is broken
		return {};
	}

	std::vector<std::reference_wrapper<const data_descriptor>> handles;
	handles.reserve(handles_unordered.size());

	std::transform(handles_unordered.begin(), handles_unordered.end(), std::back_inserter(handles),
		[](const auto& item) -> std::reference_wrapper<const data_descriptor> {
			return item.second;
		});
	handles_unordered.clear();

	// auto session = 
	// make the call below return shared_ptr<session_context>

	session_context cx(io_registry);
	size_t channel_num = collect_decompression_session_params(cx, handles);
	decompression_parameter_parser<decompression_type_params>::apply(std::move(cx), channel_num, std::move(handles));

	std::vector<bitmap<int32_t>> result;
	auto output_handles = std::move(io_registry).export_data();
	for (auto&& item : output_handles) {
		result.push_back(std::move(io_data_registry::get_data<image_selector<int32_t>>(item).image));

	}
	return result;
}


TEST(compute_graph, smoke) {
	typedef int32_t item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 12;
	props.height = 1 << 12;
	std::vector<bitmap<item_t>> input;
	input.push_back(generateNoisyBitmap<item_t>(props.width, props.height, 64));
	input.push_back(generateNoisyBitmap<item_t>(props.width, props.height, 64, 2027));
	input.push_back(generateNoisyBitmap<item_t>(props.width, props.height, 64, 713));

	compress_image(std::move(input));
}

TEST(compute_graph, file_segment_round) {
	typedef int32_t item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 9;
	props.height = 1 << 12;
	std::vector<bitmap<item_t>> input;
	// input.push_back(generateNoisyBitmap<item_t>(props.width, props.height, 64));
	// input.push_back(generateNoisyBitmap<item_t>(props.width, props.height, 64, 2027));
	input.push_back(generateNoisyBitmap<item_t>(props.width, props.height, 64, 713));

	std::filesystem::directory_iterator it(std::filesystem::current_path(),
		std::filesystem::directory_options::skip_permission_denied);
	while (it != std::filesystem::directory_iterator{}) {
		auto item_path = it->path();
		++it;
		std::filesystem::remove(item_path);
	}

	size_t target_session_id = compress_image(decltype(input)(input));
	auto output = restore_image(target_session_id, 0);
	for (ptrdiff_t i = 0; i < output.size(); ++i) {
		img_meta input_props = input[i].get_meta();

		for (size_t j = 0; j < input_props.height; ++j) {
			for (size_t k = 0; k < input_props.width; ++k) {
				EXPECT_EQ(input[i][j][k], output[i][j][k]) << " at index [" << j << "][" << k << "] for z=" << i;
			}
		}
	}
}
