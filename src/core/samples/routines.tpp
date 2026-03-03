#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <utility>

#include "core/compression.hpp"
#include "core/io.hpp"
#include "core_types.hpp"

#include "io/io_settings.hpp"
#include "io/io_contexts.hpp"
#include "io/session_context.hpp"
#include "io/io_data_registry.hpp"
#include "io/sink.hpp"

#include "io/file_protocol.hpp"

#include "dwt/dwt.hpp"
#include "dwt/segment_assembly.hpp"
#include "bpe/bpe.hpp"

// TODO: useful functions to export internal functionality of modules:
//	image dimensions padding adjustment
//


// data flow:
//	preprocess_image(bitmap)					-> std::vector<img_pos>
//	transform_fragment(bitmap, img_pos)			-> subbands_t				|= sync op, implemented as pool wait and reload
//	collect_continuous_fragments(context, id)	-> std::vector<subbands_t>
//	merge_subbands(std::vector<subbands_t>)		-> std::array<subbands_t, 2> { merged and tail }
//	assemble_segments(subbands_t)				-> std::vector<compression_context<segment_type>>
//  compress_segment(compression_context)		-> void
// 
//		dwt_descriptor:
//			img_pos target_frame
//			size_t context_id
//			state_t state
// 
//	dwt_context :
//		session_context&
//		size_t id
//		img_pos frame	// contains z/channel index
// 
//		
//		segment_descriptor:
//			img_pos target_frame
//			size_t context_id
//			state_t state
// 
//	segment_context :
//		session_context&
//		size_t id
//		subbands_t merged
//				subbands_t tail		// stored to context
//		segment incomplete_target
//			// member [always] present, empty value for the first segment in the session (with id = 0).
//			// member is object for sync. contains next target segment id. value is taken from context.
//			// value corresponding to the last segment [if image is divided into segments of given size 
//			// precisely] is put back to the context as an empty value with next id; for remaining last
//			// segment data is stored to context and then processed as special case.
//				// that is, segment assembler first populates incomplete_target segment up to target 
//				// segment size, then starts allocating new segments; the last segment is not processed 
//				// then in regards of quantization value computation and returned [half-assembled] 
//				// (essentially, packed only)
//		
//
//		compression_descriptor:
//			size_t context_id
//			size_t segment_id
//			state_t state
// 
//	compression_context:
//		session_context&
//		size_t id
//		segment
//		sink
// 
// 
// 
// 
//	preprocess_image(dwt_context)						-> std::vector<dwt_context>
//	transform_fragment(dwt_context)						-> void; pair<subbands_t, img_pos> data is put to the context				|= sync op, implemented as pool wait and reload
//		/collect_continuous_fragments(dwt_context)		-> std::vector<subbands_t> {exposition}
//		\merge_subbands(std::vector<subbands_t>)		-> segment_context
//	assemble_segments(segment_context)					-> std::vector<compression_context>
//  compress_segment(compression_context)				-> void
//


template <typename T>
struct compression_routines {
	using types = compressor_type_params<T>;
	using segment_type = types::segment_type;
	using subband_type = types::subband_type;

	template <typename iT>
	static std::vector<dwt_context> preprocess_image(dwt_context cx);

	template <typename iT>
	static void transform_fragment(dwt_context cx);

	static segmentation_context<subband_type, segment_type> preprocess_fragments(dwt_context cx);

	static std::vector<compression_context<segment_type>> assemble_segments(segmentation_context<subband_type, segment_type> cx);

	static void compress_segment(compression_context<segment_type> data);

	template <typename iT>
	static void decode_segment(compression_context<segment_type> data);

	static std::vector<segmentation_context<subband_type, segment_type>> disassemble_segments(segmentation_context<subband_type, segment_type> data);

	template <typename oT>
	static void restore_image(segmentation_context<subband_type, segment_type> data);

	template <typename oT>
	static void postprocess_image(segmentation_context<subband_type, segment_type> data);

private:
	static std::vector<subbands_t<subband_type>> collect_contiguous_fragments(dwt_context cx);

	static std::array<subbands_t<subband_type>, 2> merge_subbands(size_t image_width, std::vector<subbands_t<subband_type>>&& fragments);

	static std::vector<compression_context<segment_type>> dispatch_segments(channel_context& context, std::vector<std::unique_ptr<segment<segment_type>>>&& segments);
};

inline size_t collect_decompression_session_params(session_context& cx, std::vector<std::reference_wrapper<const data_descriptor>>& handles);


const data_descriptor& make_output_segment_descriptor(session_context& cx, size_t segment_id, size_t channel_id) {
	switch (cx.data_registry.get_segment_storage_type()) {
	// TODO: move dst_type do data_registry instance?
	case storage_type::file: {
		return cx.data_registry.put_output(cx, segment_file_descriptor{ channel_id, segment_id});
		break;
	}
	case storage_type::memory: {
		return cx.data_registry.put_output(cx, segment_memory_descriptor{ channel_id, segment_id });
		break;
	}
	default: {
		// TODO: error handling
		break;
	}
	}
}

template <typename T>
template <typename iT>
std::vector<dwt_context> compression_routines<T>::preprocess_image(dwt_context cx) {
	auto op_state_token = cx.channel_cx.descriptors.start_operation(cx);
	auto& transformer = per_thread::compressors<T>::get_transformer(cx.channel_cx.session_cx);

	image_memory_descriptor<iT>& descriptor = io_data_registry::get_data<image_selector<iT>>(cx.data);
	bitmap<iT>& img = descriptor.image;

	if (cx.channel_cx.session_cx.settings_session.transpose) {
		img = img.transpose();
	}
	transformer.preprocess_image(img);

	std::vector<dwt_context> frames;

	size_t target_segment_size = 0;
	size_t image_width = img.get_meta().width; // context.settings_session.img_width;
	size_t max_frame_width = image_width;
	constexpr size_t padding_requirement = 8;
	max_frame_width = (max_frame_width + padding_requirement - 1) & (~(padding_requirement - 1));
	size_t executors_count = 4; // TODO:
	// size_t img_block_count = ((img.get_meta().width + 7) >> 3) * ((img.get_meta().height + 7) >> 3);
	size_t largest_segment_size = 0;

	// TODO: here we should defend against concurrent collection access
	auto largest_segment = std::max_element(cx.channel_cx.session_cx.seg_settings.cbegin(), cx.channel_cx.session_cx.seg_settings.cend(),
		[](const std::pair<size_t, segment_settings>& lhs, const std::pair<size_t, segment_settings>& rhs) -> bool {
			return lhs.second.size < rhs.second.size;
		});
	if (largest_segment != cx.channel_cx.session_cx.seg_settings.cend()) {
		largest_segment_size = largest_segment->second.size;
	} else {
		largest_segment_size = max_frame_width;
	}

	ptrdiff_t frame_width = std::min(max_frame_width, largest_segment_size);
	ptrdiff_t frame_height = img.get_meta().height / executors_count;
	constexpr ptrdiff_t height_granularity = 256;
	constexpr ptrdiff_t height_ceiling_factor = 128;
	frame_height = (frame_height < height_granularity) ? height_granularity : frame_height;
	frame_height = (frame_height + height_ceiling_factor) & (~(height_granularity - 1));

	ptrdiff_t x = 0, y = 0;
	while (y < img.get_meta().height) {
		img_pos frame_item;
		frame_item.x = x;
		frame_item.y = y;
		frame_item.z = cx.frame.z;
		frame_item.height = frame_height;
		frame_item.width = frame_width;
		frame_item.depth = 1;

		frames.emplace_back(generate_dwt_id(), cx.channel_cx, frame_item, cx.data);

		x += frame_width;

		bool right_edge_reached = (x >= image_width);
		x -= zeropred(image_width, right_edge_reached);
		y += zeropred(frame_height, right_edge_reached);
	}

	img_pos& last_frame = frames.back().frame;
	last_frame.width = image_width - last_frame.x;

	size_t image_height = img.get_meta().height;
	image_height = (image_height + padding_requirement - 1) & (~(padding_requirement - 1));

	for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
		bool bottom_edge_overlapped = (it->frame.y + it->frame.height) > image_height;
		it->frame.height -= zeropred((it->frame.y + it->frame.height) - image_height, bottom_edge_overlapped);
		if (!bottom_edge_overlapped) {
			break;
		}
	}

	cx.channel_cx.descriptors.register_operations(frames);
	return frames;
}

template <typename T>
template <typename iT>
void compression_routines<T>::transform_fragment(dwt_context cx) {
	auto op_state_token = cx.channel_cx.descriptors.start_operation(cx);
	auto& transformer = per_thread::compressors<T>::get_transformer(cx.channel_cx.session_cx);

	if (cx.channel_cx.session_cx.settings_session.custom_shifts) {
		transformer.get_scale().set_shifts(cx.channel_cx.session_cx.settings_session.shifts);
	}

	image_memory_descriptor<iT>& descriptor = io_data_registry::get_data<image_selector<iT>>(cx.data);
	bitmap<iT>& img = descriptor.image;

	auto result_subbands = transformer.apply(img, cx.frame);

	auto& data = std::get<compression_data<T>>(cx.channel_cx.data);

	std::lock_guard lock(data.subband_fragments_mx);
	data.subband_fragments.emplace_back(std::move(result_subbands), cx.frame);
}

template <typename T>
segmentation_context<typename compression_routines<T>::subband_type, typename compression_routines<T>::segment_type> 
compression_routines<T>::preprocess_fragments(dwt_context cx) {
	auto op_state_token = cx.channel_cx.descriptors.start_operation(cx);
	cx.channel_cx.session_cx.data_registry.free_descriptor(cx.data);
	size_t image_width = cx.channel_cx.session_cx.settings_session.img_width;
	image_width = ((image_width + 7) >> 3) << 3; // TODO: we should account for padding...

	auto [merged, tail] = merge_subbands(image_width, collect_contiguous_fragments(cx));
	img_pos merged_frame = merged[0].single_frame_params();
	merged_frame.width <<= 3;
	merged_frame.height <<= 3;
	// frame is expected to be the width of the whole image, therefore no adjustments needed 
	// for x axis; preprocessing is performed per channel, therefore no adjustments for z axis
	
	if (tail[0].get_meta().width > 0) {
		img_pos tail_description = cx.frame;
		tail_description.y += merged_frame.height;
		tail_description.height -= merged_frame.height;

		auto& data = std::get<compression_data<T>>(cx.channel_cx.data);

		std::lock_guard lock(data.subband_fragments_mx);
		data.subband_fragments.emplace_front(tail, tail_description);
	}

	merged_frame.x = cx.frame.x;
	merged_frame.y = cx.frame.y;
	merged_frame.z = cx.frame.z;
	
	segmentation_context<subband_type, segment_type> result {
		generate_segmentation_id(),
		cx.channel_cx,
		std::move(merged),
		std::make_unique<segment<segment_type>>(), 
		merged_frame
	};

	cx.channel_cx.descriptors.register_operation(result);
	return result;
}

template <typename T>
std::vector<subbands_t<typename compression_routines<T>::subband_type>> compression_routines<T>::collect_contiguous_fragments(dwt_context cx) {
	auto& data = std::get<compression_data<T>>(cx.channel_cx.data);

	using view_iterator_t = decltype(data.subband_fragments)::const_iterator;
	std::vector<view_iterator_t> fragments_view;

	view_iterator_t view_begin_it;
	view_iterator_t view_end_it;
	size_t view_size = 0;
	{
		std::lock_guard lock(data.subband_fragments_mx);
		view_begin_it = data.subband_fragments.cbegin();
		view_end_it = data.subband_fragments.cend();
		view_size = data.subband_fragments.size();
		
		[[unlikely]]
		if (view_size == 0) {
			// TODO: error handling, likely exception
		} else {
			view_end_it = std::prev(view_end_it);
		}
	}

	// iterators are valid. The current context is only modifier that extracts subbands from the collection
	fragments_view.reserve(view_size);	// TODO: sync, up to completion of local iterator collection
	for (auto it = view_begin_it; it != view_end_it; ++it) {
		fragments_view.push_back(it);
	}
	fragments_view.push_back(view_end_it);

	// implementation using partitions was necessary for global subband_fragments collection 
	// that contained fragments for all image channels.
	auto sorted_begin = std::partition(fragments_view.begin(), fragments_view.end(),
		[z = cx.frame.z](const auto& it) -> bool {
			return it->second.z < z;
		});
	auto sorted_end = std::partition(sorted_begin, fragments_view.end(),
		[z = cx.frame.z](const auto& it) -> bool {
			return !(it->second.z > z);
		});

	std::sort(sorted_begin, sorted_end,
		[](decltype(fragments_view)::const_reference lhs, decltype(fragments_view)::const_reference rhs) 
				-> bool {
		// [](const fragment_description_t& lhs, const fragment_description_t& rhs) -> bool {
			// return 
			// 	(lhs->second.z < rhs->second.z) |
			// 	(
			// 		(lhs->second.z == rhs->second.z) &
			// 		(
			// 			(lhs->second.y < rhs->second.y) |
			// 			(
			// 				(lhs->second.y == rhs->second.y) &
			// 				(lhs->second.x < rhs->second.x)
			// 			)
			// 		)  
			// 	); 

			return
				(lhs->second.y < rhs->second.y) |
				(
					(lhs->second.y == rhs->second.y) &
					(lhs->second.x < rhs->second.x)
				);
		});

	decltype(data.subband_fragments) splice_buffer;
	// TODO: current implementation does not support merging subbands in several passes, 
	// only whole image at once.
	size_t next_x = cx.frame.x;
	size_t next_y = cx.frame.y;
	size_t img_width = cx.channel_cx.session_cx.settings_session.img_width;
	// TODO: adjust for padding
	constexpr size_t padding_requirement = 8;
	img_width = (img_width + padding_requirement - 1) & (~(padding_requirement - 1));

	typename decltype(fragments_view)::iterator view_it = std::partition_point(
		sorted_begin, sorted_end,
		[x = next_x, y = next_y, z = cx.frame.z](const auto& item) -> bool {
			// return
			// 	(item->second.z < z) |
			// 	(
			// 		(item->second.z == z) &
			// 		(
			// 			(item->second.y < y) |
			// 			(
			// 				(item->second.y == y) &
			// 				(item->second.x < x)
			// 			)
			// 		)
			// 	);

			return
				(item->second.y < y) |
				(
					(item->second.y == y) &
					(item->second.x < x)
				);
		});

	{
		std::lock_guard lock(data.subband_fragments_mx);

		while ((view_it != sorted_end) &&
				(((*view_it)->second.x == next_x) & ((*view_it)->second.y == next_y))) {
			view_iterator_t& it = *view_it;
			
			// splice for single item is expected in constant time
			splice_buffer.splice(splice_buffer.end(), data.subband_fragments, it);
			// splice does not invalidate the iterator, however the iterators obtained from 
			// global subband_fragments will become invalidated on splice_buffer destruction.
			// 
			// As subband_fragments buffers are managed per image channel, no concurrent access 
			// to collection iterators is expected, and it should be safe to destroy local 
			// splice_buffer at the end of the routine, e.g. invalidate some iterators from 
			// global subband_fragments.
			// 
			
			next_x += it->second.width;

			bool right_edge_reached = (next_x >= img_width);
			next_x -= zeropred(img_width, right_edge_reached);
			next_y += zeropred(it->second.height, right_edge_reached);

			++view_it;
		}
	}

	// output items are sorted and are going to be merged. Transfer elements to vector
	std::vector<subbands_t<subband_type>> result;
	std::for_each(splice_buffer.begin(), splice_buffer.end(), 
		[&result](auto& item) -> void {
			result.push_back(std::move(item.first));
		});

	return result;
}

template <typename T>
std::array<subbands_t<typename compression_routines<T>::subband_type>, 2> compression_routines<T>::merge_subbands(size_t image_width, std::vector<subbands_t<typename compression_routines<T>::subband_type>>&& fragments) {
	// img width is needed to merge into continious subband fragment. first fragment 
	// x position is needed; but it can be replaced with subbands_t instance of the tail 
	// of the previous merge_subbands call, and the result thus can be included in the 
	// main output subbands fragment
	// but such argument can be the first item in the input vector.
	// 
	// it may make sense to return std::array<subbands_t, 3> with 3 rectangular regions:
	// [0] fragment of smaller width than the img width, located along the right edge of 
	//		the image, has left overlap region. It can be used to merge with another 
	//		merge_subbands output fragment to build continious
	// [1]	main continious rectangular fragment with width = img width
	// [2]	tail that didn't have enough data to make the whole row
	// 
	// makes sense to return std::array<subbands_t, 2>:
	// [0]	main rectangular full width fragment
	// [1]	tail, should be used to subsequent call to merge_subbands
	//

	// Current segment assembly implementation consumes subband buffers, i.e. takes by 
	// rvalue reference and frees all buffers associated with row dwt coefficients data. 
	// The last segment in the subband buffer is interpreted as the last coefficient of 
	// the input image.
	// This implies limitations on segment processing of fragmented image; only whole 
	// image data segmentation at once is supported. This means that start of bpe 
	// processing is blocked by segment processing, and start of segment processing is 
	// blocked by whole image dwt transform; i.e. after the image is dwt-transformed 
	// (possibly in fragmented manner), there is a single task of segment processing
	// in the scheduling queue, and all executers but one are forced to move into 
	// idle state until segment processing is completed and encoding tasks are put 
	// in the global scheduling queue.
	// We could do the following to support fragmented segmentation:
	//	implement additional flag in segment assembler to indicate that the last segment 
	//		built from the current subband set should not be further processed after 
	//		block values are packed (that is, not interpreted as the last segment of the 
	//		input image); segment data should preserve original block coefficients
	//	add special support for segment completion as part of regular apply call. Segment 
	//		is taken as additional call parameter, and it's being filled up to regular 
	//		segment size (using data from provided subbands) before other segments are 
	//		allocated.
	//	add data member as part of session context for unfinished segment data
	//	copy subbands region or split accumulated subband data, pass one subband set to 
	//		segment assembler (if bitmap<T> apply overload is used). As an alternative, 
	//		implement bitmap_slice overload for segment assembler apply call.
	//

	// precondition: all input subband sets have the same height
	bool valid = true;
	valid &= !fragments.empty();
	if (!valid) {
		// TODO: error handling
	}

	size_t fragment_height = fragments[0][0].get_meta().height << 3;

	subbands_t<subband_type> tail;

	// TODO: make as part of subbands_t interface
	auto resize_subbands = [](subbands_t<subband_type>& subbands, size_t img_width, size_t img_height) -> void {
		ptrdiff_t i = -1;
		for (ptrdiff_t gen = constants::subband::generation_num; gen > 0; --gen) {
			size_t subband_width = img_width >> gen;
			size_t subband_height = img_height >> gen;

			ptrdiff_t base_index = subbands.size() - constants::subband::subbands_per_generation * gen;
			for (; i < ((ptrdiff_t)constants::subband::subbands_per_generation); ++i) {
				subbands[base_index + i].resize(subband_width, subband_height);
			}
			i = 0;
		}
	};

	size_t region_height = 0;
	size_t region_fragment_count = 0;
	{
		size_t current_region_width = 0;
		size_t current_row_fragment_count = 0;
		size_t last_crossrow_overrun = 0;
		for (auto& item : fragments) {
			size_t item_width = item[0].get_meta().width << 3;
			size_t item_height = item[0].get_meta().height << 3;
			current_region_width += item_width;

			bool right_edge_reached = (current_region_width >= image_width);
			region_height += zeropred(item_height, right_edge_reached);
			current_region_width -= zeropred(image_width, right_edge_reached);

			++current_row_fragment_count;
			region_fragment_count += zeropred(current_row_fragment_count, right_edge_reached);
			current_row_fragment_count = zeropred(current_row_fragment_count, !right_edge_reached);
			last_crossrow_overrun = right_edge_reached ? current_region_width : last_crossrow_overrun;

			if (item_height != fragment_height) {
				// TODO: fragments are not equal in height. Should we handle it?
			}
		}

		resize_subbands(tail, current_region_width, fragment_height);

		img_pos tail_frame;
		tail_frame.x = last_crossrow_overrun;
		tail_frame.y = 0;
		for (ptrdiff_t i = region_fragment_count; i < fragments.size(); ++i) {
			tail_frame.x_step = fragments[i][0].get_meta().width << 3;
			ptrdiff_t j = -1;
			for (ptrdiff_t gen = constants::subband::generation_num; gen > 0; --gen) {
				// For some reason, msvc implementation defines rank for size_t greater than that 
				// for ptrdiff_t, which leads to unsigned result of implicit conversions...
				//
				// is it reasonable to check all the code and move to another type for indexing?
				for (; j < ((ptrdiff_t)constants::subband::subbands_per_generation); ++j) {
					ptrdiff_t subband_index = fragments[i].size() - gen * constants::subband::subbands_per_generation + j;
					img_pos item_frame = fragments[i][subband_index].single_frame_params();
					img_pos dst_frame = item_frame;

					dst_frame.x = tail_frame.x >> gen;

					tail[subband_index].slice(dst_frame).assign(fragments[i][subband_index].slice(item_frame));
				}
				j = 0;
			}

			tail_frame.x += tail_frame.x_step;
		}
		if (last_crossrow_overrun > 0) {
			subbands_t<subband_type>& last_region_fragment = fragments[region_fragment_count - 1];
			ptrdiff_t j = -1;
			for (ptrdiff_t gen = constants::subband::generation_num; gen > 0; --gen) {
				for (; j < ((ptrdiff_t)constants::subband::subbands_per_generation); ++j) {
					ptrdiff_t subband_index = last_region_fragment.size() - gen * constants::subband::subbands_per_generation + j;
					img_pos item_frame = last_region_fragment[subband_index].single_frame_params();
					item_frame.x = (last_crossrow_overrun >> gen);
					item_frame.width = item_frame.width - item_frame.x;

					img_pos dst_frame = item_frame;
					dst_frame.x = 0;

					tail[subband_index].slice(dst_frame).assign(last_region_fragment[subband_index].slice(item_frame));

					// and adjust last fragment item's dimensions
					auto fragment_dims = last_region_fragment[subband_index].get_meta();
					fragment_dims.width = fragment_dims.width - item_frame.x;

					// here we bet on bitmap data preservation (dimensions decreased)
					last_region_fragment[subband_index].resize(fragment_dims.width, fragment_dims.height);
				}
				j = 0;
			}
		}
		// here fragment buffers [region_fragment_count-1, ] can be released
	}

	subbands_t<subband_type> merged;
	resize_subbands(merged, image_width, region_height);

	img_pos image_frame;
	image_frame.x = 0;
	image_frame.y = 0;
	image_frame.y_step = fragment_height;
	for (ptrdiff_t i = 0; i < region_fragment_count; ++i) {
		auto& item = fragments[i];

		image_frame.x_step = item[0].get_meta().width << 3;
		ptrdiff_t j = -1;
		for (ptrdiff_t gen = constants::subband::generation_num; gen > 0; --gen) {
			for (; j < ((ptrdiff_t)constants::subband::subbands_per_generation); ++j) {
				ptrdiff_t subband_index = item.size() - gen * constants::subband::subbands_per_generation + j;
				img_pos item_frame = item[subband_index].single_frame_params();
				img_pos dst_frame = item_frame;

				size_t item_subband_width = item[subband_index].get_meta().width;
				size_t merged_subband_width = merged[subband_index].get_meta().width;
				dst_frame.x = image_frame.x >> gen;
				dst_frame.y = image_frame.y >> gen;
				dst_frame.y_step = item_frame.height;

				bool right_edge_reached = false;

				do {
					item_frame.width = std::min(merged_subband_width - dst_frame.x, item_subband_width - item_frame.x);
					item_frame.x_step = item_frame.width;
					// below works fine because dwt frame is no wider than the image
					item_frame.height = fragments[i + right_edge_reached][subband_index].get_meta().height;
					dst_frame.width = item_frame.width;
					dst_frame.x_step = item_frame.x_step;
					dst_frame.height = item_frame.height;

					merged[subband_index].slice(dst_frame).assign(item[subband_index].slice(item_frame));

					item_frame.x += item_frame.x_step;
					dst_frame.x += dst_frame.x_step;

					right_edge_reached = (dst_frame.x >= merged_subband_width);
					dst_frame.x = zeropred(dst_frame.x, !right_edge_reached);
					dst_frame.y += zeropred(dst_frame.y_step, right_edge_reached);
				} while (item_frame.x < item_subband_width);
			}
			j = 0;
		}
		image_frame.x += image_frame.x_step;
		{
			bool right_edge_reached = (image_frame.x >= image_width);
			image_frame.x = zeropred(image_frame.x, !right_edge_reached);
			image_frame.y += zeropred(image_frame.y_step, right_edge_reached);
		}
	}

	// fragments [region_fragment_count, ] are tail, need to store them temporarily somewhere
	return { merged, tail };
}

template <typename T>
std::vector<compression_context<typename compression_routines<T>::segment_type>> compression_routines<T>::assemble_segments(segmentation_context<typename compression_routines<T>::subband_type, typename compression_routines<T>::segment_type> cx) {
	auto op_state_token = cx.channel_cx.descriptors.start_operation(cx);
	// input subbands is a rectangular continious fragment of input image with the width 
	// equal to the source image. 
	// or it may be accumulative subband container, containing transformed so far image 
	// regions beginning from the image top-left.
	// 
	// previous assembly call stop coordinates needed: the position where to start building 
	// first segment in the output sequence.
	// should somehow save the last item location parameters.
	// 
	// returns collection of segments (or just appends them to the context)
	//
	bool custom_shifts = cx.channel_cx.session_cx.settings_session.custom_shifts;
	shifts_t dwt_shifts = cx.channel_cx.session_cx.settings_session.shifts;

	auto& assembler = per_thread::compressors<T>::get_assembler(cx.channel_cx.session_cx);
	// TODO: add apply(subbands_t&, size_t count) interface to segment assembler to support 
	// variadic segment sizes

	auto [settings, strict_match] = get_segment_settings(cx.channel_cx.session_cx, cx.incomplete_segment_data->id); // TODO: segmend id
	assembler.set_shifts(dwt_shifts);
	assembler.set_segment_size(settings.size);
	// TODO: here we should split assembler.apply calls according to consecutive segment_settings's
	// segment size values, so that segments are built with different sizes; see the comment above
	// 
	// cx.incomplete_segment_data is kind of serializing token, it should be passed to consecutive 
	// assemle_segments call with updated data; i.e. it has to be session_context member.
	//

	auto result = dispatch_segments(cx.channel_cx, assembler.apply(std::move(cx.subband_data)));
	cx.channel_cx.descriptors.register_operations(result);
	return result;
}

template <typename T>
std::vector<compression_context<typename compression_routines<T>::segment_type>> compression_routines<T>::dispatch_segments(channel_context& context, std::vector<std::unique_ptr<segment<typename compression_routines<T>::segment_type>>>&& segments) {
	// handle segment preprocessing work: allocate sink, make necessary sink configs
	// 
	context.free_compressed_segment_data();

	constexpr size_t id_volume_limit = 256;

	auto& data = std::get<compression_data<T>>(context.data);

	std::pair<size_t, size_t> available_id_range { 
		context.allocated_segment_id.second,
		context.allocated_segment_id.first + id_volume_limit
	};
	// size_t next_available_id = std::min(available_id_range.second,
	// 	context.allocated_segment_id.second + data.tail.size() + segments.size());
	size_t next_available_id = context.allocated_segment_id.second + data.tail.size() + segments.size();
	context.allocated_segment_id.second = next_available_id;
	
	std::vector<compression_context<segment_type>> dispatched_segments;
	std::vector<compression_descriptor> dispatched_descriptors;

	auto make_contexts = [&](auto& collection) /*-> {iterator}*/ {
		size_t available_id_range_length = available_id_range.second - available_id_range.first;
		// size_t segment_num_to_dispatch = std::min(available_id_range_length, collection.size());
		size_t segment_num_to_dispatch = collection.size();
		auto it = collection.begin();
		for (ptrdiff_t i = 0; i < segment_num_to_dispatch; ++i, ++it) {
			size_t segment_id = available_id_range.first + i;
			size_t context_id = generate_compression_id();
			(*it)->id = segment_id;
			compression_context<segment_type> segment_context {
				context_id, 
				context,
				//make_sink<compression_context<segment_type>::sink_value_type>(context.session_cx, context.session_cx.dst_type, segment_id, context.channel_index),
				std::move(*it),
				make_output_segment_descriptor(context.session_cx, segment_id, context.channel_index)
			};
			dispatched_segments.push_back(std::move(segment_context));
		}

		available_id_range.first += segment_num_to_dispatch;
		return it;
	};


	{
		decltype(data.tail) local_tail;
		{
			std::lock_guard lock(data.tail_mx);
			local_tail.swap(data.tail);
		}

		auto tail_start_it = make_contexts(local_tail);
		local_tail.erase(local_tail.cbegin(), tail_start_it);
		{
			std::lock_guard lock(data.tail_mx);
			data.tail.splice(data.tail.cbegin(), local_tail);
		}
	}

	{
		auto tail_start_it = make_contexts(segments);
		{
			std::lock_guard lock(data.tail_mx);
			std::move(tail_start_it, segments.end(), std::back_inserter(data.tail));
			// TODO: move to local buffer first, then splice to global tail end?
		}
	}

	// TODO: refactor special segments hanlers when control flow is redesigned
	if (!dispatched_segments.empty()) {
		dispatched_segments.front().channel_start = true;
		dispatched_segments.back().channel_end = true;

		if (context.channel_index == 0) {
			dispatched_segments.front().image_start = true;
		}
		if (context.channel_index == (context.session_cx.channel_contexts.size() - 1)) {
				// channel_contexts at least contains single element that has to be context
			dispatched_segments.back().image_end = true;
		}
	}

	context.descriptors.register_operations(dispatched_segments);
	return dispatched_segments;
}

template <typename T>
void compression_routines<T>::compress_segment(compression_context<typename compression_routines<T>::segment_type> cx) {
	auto op_state_token = cx.channel_cx.descriptors.start_operation(cx);
	auto& encoder = per_thread::compressors<T>::get_encoder(cx.channel_cx.session_cx);
	auto [param_compr, flag_compr] = get_compression_settings(cx.channel_cx.session_cx, cx.segment_data->id);
	auto [param_seg, flag_seg] = get_segment_settings(cx.channel_cx.session_cx, cx.segment_data->id);

	encoder.set_use_heuristic_DC(param_seg.heuristic_quant_DC);
	encoder.set_use_heuristic_bdepthAc(param_seg.heuristic_bdepth_AC);
	if (param_compr.early_termination) {
		encoder.set_stop_after_DC(param_compr.DC_stop);
		encoder.set_stop_at_bplane(param_compr.bplane_stop, param_compr.stage_stop);
	} else {
		// refactor default params
		encoder.set_stop_after_DC(false);
		encoder.set_stop_at_bplane(0, 0b11);
	}

	auto sink = make_sink<size_t>(cx.channel_cx.session_cx, cx.data);

	ccsds_file_protocol specific_protocol(*(cx.segment_data), cx.segment_data->id);

	if (cx.image_start) {
		bool valid = true;
		valid &= flag_compr;
		valid &= flag_seg;

		if (!valid) {
			// TODO: error handling
		}

		specific_protocol.init_session(cx.channel_cx.session_cx.settings_session, param_seg, param_compr);
	} else {
		specific_protocol.set_first(cx.channel_start);
		if (flag_seg) {
			specific_protocol.set_segment_params(param_seg);
		}
		if (flag_compr) {
			specific_protocol.set_compression_params(param_compr);
		}
	}

	if (cx.channel_end) {
		specific_protocol.set_last(cx.channel_cx.session_cx.settings_session.rows_pad_count);

		size_t image_width = cx.channel_cx.session_cx.settings_session.img_width;
		// cast to LL3 dims
		image_width = (image_width + ((1 << 3) - 1)) >> 3;
		if (param_seg.size > image_width) {
			// if the segment length is greater than the image width, it is necessary to 
			// specify actual segment size via segment header part 3. Otherwise it's not 
			// possible to decode image demensions unambigiously and correctly.
			//

			segment_settings last_segment_settings = param_seg;
			last_segment_settings.size = cx.segment_data->size;
			specific_protocol.set_segment_params(last_segment_settings);
		}
		// no special handling for cx.image_end
	}

	sink->setup_session();

	try {
		specific_protocol.commit(sink->get_bitwrapper(), param_compr);
		encoder.encode(*(cx.segment_data), sink->get_bitwrapper());
		sink->finish_session();
	} catch (const ccsds::bpe::byte_limit_exception& ex) {
		sink->handle_early_termination();
	}

	specific_protocol.set_segment_size(sink->get_bitwrapper().get_byte_count());
	// TODO: override protocol header?
}



template <typename T>
template <typename iT>
void compression_routines<T>::decode_segment(compression_context<segment_type> cx) {
	auto op_state_token = cx.channel_cx.descriptors.start_operation(cx);
	auto& decoder = per_thread::decompressors<T>::get_decoder(cx.channel_cx.session_cx);

	auto [param_compr, flag_compr] = get_compression_settings(cx.channel_cx.session_cx, cx.segment_data->id);
	auto [param_seg, flag_seg] = get_segment_settings(cx.channel_cx.session_cx, cx.segment_data->id);

	cx.segment_data->size = param_seg.size;
	cx.segment_data->bit_shifts = cx.channel_cx.session_cx.settings_session.shifts;

	auto src = make_source<iT>(cx.channel_cx.session_cx, cx.data);
	src->setup_session();

	try {
		auto specific_protocol = ccsds_header_parser<ccsds_file_protocol>::parse_header(src->get_bitwrapper());
		
		constexpr size_t segment_count_mask = (1 << 8) - 1;

		bool valid = true;
		valid &= !(flag_compr ^ specific_protocol.if_contains_compression_params());
		valid &= !(flag_seg ^ specific_protocol.if_contains_segment_params());
		valid &= (specific_protocol.get_segment_count() == (cx.segment_data->id & segment_count_mask));
		if (!valid) {
			// TODO: handle error
		}

		cx.segment_data->bdepthAc = specific_protocol.get_bit_depth_AC();
		cx.segment_data->bdepthDc = specific_protocol.get_bit_depth_DC();

		if (specific_protocol.if_last()) {
			// the standard does not require to include header part 3 for the last segment if its 
			// size is less than the effectively encoded expected segment size in the last header 
			// part 3. Thus it is necessary to compute encoded image dimensions at this point, so 
			// that true last segment size can be set in segment structure, to prevent corruption 
			// during decoding.
			//
			// However, the last segment must contain header part 3 if effective segment size lays 
			// across multiple (more than 1) image rows. Otherwise it is not possible to properly 
			// decode the segment and compute original image height.
			//

			size_t rows_pad_count = specific_protocol.get_pad_rows_count();
			size_t image_width = cx.channel_cx.session_cx.settings_session.img_width;
			uintmax_t blocks_total = 0;

			const auto& segmentation_settings = cx.channel_cx.session_cx.seg_settings;
			size_t next_setting_segment_id = cx.segment_data->id;

			std::for_each(segmentation_settings.crbegin(), segmentation_settings.crend(),
				[&next_setting_segment_id, &blocks_total](const auto& item_pair) -> void {
					blocks_total += item_pair.second.size * (next_setting_segment_id - item_pair.first);
					next_setting_segment_id = item_pair.first;
				});

			// cast to LL3 dims
			// TODO: account padding needed here?
			image_width = (image_width + ((1 << 3) - 1)) >> 3;
			size_t image_height = (blocks_total + param_seg.size) / image_width;
			cx.segment_data->size = image_height * image_width - blocks_total;
			// cx.channel_cx.session_cx.settings_session.rows_pad_count = rows_pad_count;
		}

		if (param_compr.early_termination) {
			decoder.set_stop_after_DC(param_compr.DC_stop);
			decoder.set_stop_at_bplane(param_compr.bplane_stop, param_compr.stage_stop);
		} else {
			// refactor default params
			decoder.set_stop_after_DC(false);
			decoder.set_stop_at_bplane(0, 0b11);
		}

		decoder.decode(*(cx.segment_data), src->get_bitwrapper());
	} catch (const ccsds::bpe::byte_limit_exception& ex) {
		// TODO: error handling? is this scenario needed?
	}

	src.reset();
	cx.channel_cx.session_cx.data_registry.free_descriptor(cx.data);

	auto& data = std::get<compression_data<T>>(cx.channel_cx.data);
	{
		std::lock_guard lock(data.tail_mx);
		data.tail.push_back(std::move(cx.segment_data));
	}
}

template <typename T>
std::vector<segmentation_context<typename compression_routines<T>::subband_type, typename compression_routines<T>::segment_type>> 
compression_routines<T>::disassemble_segments(segmentation_context<subband_type, segment_type> cx) {
	auto op_state_token = cx.channel_cx.descriptors.start_operation(cx);
	auto& data = std::get<compression_data<T>>(cx.channel_cx.data);

	decltype(data.tail) local_tail;
	{
		std::lock_guard lock(data.tail_mx);
		local_tail.swap(data.tail);
	}
	std::vector<decltype(local_tail)::value_type> segments;
	segments.reserve(local_tail.size());
	std::move(local_tail.begin(), local_tail.end(), std::back_inserter(segments));

	std::sort(segments.begin(), segments.end(), 
		[](const auto& lhs, const auto& rhs) -> bool {
			return lhs->id < rhs->id;
		});

	bool valid = true;
	valid &= !segments.empty();
	if (!valid) {
		// TODO: error handling
	}

	auto it = segments.cbegin();
	size_t prev_segment_id = (*it)->id;

	valid &= ((*it)->id == 0);
	it++;
	std::for_each(it, segments.cend(),
		[&prev_segment_id, &valid](const auto& item) -> void {
			++prev_segment_id;
			valid &= (item->id == prev_segment_id);
		});
	if (!valid) {
		// TODO: error handling, segment id sequence is broken
	}

	auto& disassembler = per_thread::decompressors<T>::get_disassembler(cx.channel_cx.session_cx);

	// TODO: 
	// segment disassembler inherently returns collection of subbands for target image fragments, 
	// because target image height is not known yet for the general case (and for concurrent 
	// processing in particular). This incourages using concurrent backward dwt transform for 
	// corresponding image fragments, with a subsequent image postprocessing, comprised of
	// fragments merging and rows/columns padding cuts.
	// As for now, fragment-aware implementations of backward dwt and segment disassembly are 
	// not compatible: segment disassembler adds top overlay for every subband item proportionally 
	// to some target image dimension overlay, and backward dwt optimizes internal buffers dimension 
	// for minimal overlay required for correct transformation; this causes incompatible [subband 
	// item] and [dwt internal buffer] dimensions, and dwt transform is not applicable to those when 
	// combined in same backward dwt pass.
	// Use dummy subband merging logic from test cases temporarily. Segment assembly and dwt are to 
	// be refactored slightly, implement compatible logic then. 
	// 
	// Proposed implementation:
	// subband fragments are to be stored in consumer contexts, segmentation task splits on those 
	// contexts. Each restored image fragment is stored in output data registry; output handles/
	// descriptors are stored in a dedicated collection in channel context, access sync is needed.
	// Image postprocessing is performed when all backward dwt tasks are completed; image fragments 
	// are extracted from descriptor collection {, maybe sorted} and merged into target image.
	// In backward dwt implementation, it may be needed to reduce input subband items' dimensions:
	// cut furst rows and cut first columns. Those can be effectively implemented for current 
	// bitmap implementation, however, image data alignment is to be broken for columns cut, and 
	// care should be taken for image data buffer end and image stride/past-the-end buffers/offsets 
	// handling.
	// It may be reasonable to implement bitmap cuts as part of bitmap view/splice interface, and 
	// move non-modifying transformations (transpose and copy-cast) there as well.
	//

	size_t image_width = cx.channel_cx.session_cx.settings_session.img_width;
	disassembler.set_image_width(image_width);
	// auto merged = merge_subbands(image_width, std::move(disassembler.apply(std::move(segments))));
	// cx.subband_data = std::move(merged[0]);
	// cx.frame = cx.subband_data[0].single_frame_params();
	// 
	// // tail dimension should be 0, whole image is expected to be decoded at this point
	// auto tail_dims = merged[1][0].get_meta();
	// 
	// valid &= (tail_dims.width == 0);
	// valid &= (tail_dims.height == 0);
	// if (!valid) {
	// 	// TODO: error handling
	// }
	// 
	// // reuse context
	// op_state_token.unlock();
	// cx.id = generate_segmentation_id();
	// cx.channel_cx.descriptors.register_operation(cx);
	// 
	// return cx;

	decltype(segments) segments_copy(segments.size());
	for (ptrdiff_t i = 0; i < segments.size(); ++i) {
		segments_copy[i] = std::make_unique<segment<segment_type>>(*segments[i]);
	}

	auto fragments = disassembler.apply(std::move(segments));
	std::vector<segmentation_context<subband_type, segment_type>> result;
	result.reserve(fragments.size());
	size_t fragment_y = 0;
	constexpr size_t top_overlap = 4;
	constexpr size_t bottom_overlap = 3;
	bool first_fragment = true;
	// for (auto&& item : fragments) {
	for (ptrdiff_t i = 0; i < fragments.size(); ++i) {
		auto& item = fragments[i];
		size_t fragment_top_overlap = zeropred(top_overlap, !first_fragment);
		img_pos fragment_frame = item[0].single_frame_params();
		fragment_frame.y = fragment_y;
		fragment_frame.height -= fragment_top_overlap;
		fragment_frame.height -= bottom_overlap;
		fragment_frame.height <<= 3;
		fragment_frame.width <<= 3;

		segmentation_context<subband_type, segment_type> fragment_context{
			generate_segmentation_id(), 
			cx.channel_cx, 
			std::move(item), 
			std::move(segments_copy[i]),
			fragment_frame, 
			fragment_top_overlap,
			bottom_overlap
		};
		result.push_back(std::move(fragment_context));

		fragment_y += fragment_frame.height;
		first_fragment = false;
	}
	// result.back().frame.height += result.back().bottom_overlap << 3;
	// result.back().bottom_overlap = 0;
	result.back().frame.height = (fragments.back()[0].get_meta().height - result.back().top_overlap) << 3;
	result.back().bottom_overlap = 0;

	{
		std::lock_guard lock(data.fragments_mx);
		data.fragments.reserve(result.size());
	}

	cx.channel_cx.descriptors.register_operations(result);
	return result;
}

template <typename T>
template <typename oT>
void compression_routines<T>::restore_image(segmentation_context<subband_type, segment_type> cx) {
	auto op_state_token = cx.channel_cx.descriptors.start_operation(cx);
	auto& transformer = per_thread::decompressors<T>::get_transformer(cx.channel_cx.session_cx);

	if (cx.channel_cx.session_cx.settings_session.custom_shifts) {
		transformer.get_scale().set_shifts(cx.channel_cx.session_cx.settings_session.shifts);
	}

	auto& data = std::get<compression_data<T>>(cx.channel_cx.data);

	auto fragment = transformer.apply<oT>(cx.subband_data, cx.top_overlap);
	fragment.shrink(fragment.get_meta().height - (cx.bottom_overlap << 3));

	image_memory_descriptor<oT> descriptor_data(std::move(fragment), cx.channel_cx.channel_index);

	const data_descriptor& descriptor = 
		cx.channel_cx.session_cx.data_registry.put_output(cx.channel_cx.session_cx, std::move(descriptor_data));

	{
		std::lock_guard lock(data.fragments_mx);
		data.fragments.emplace_back(cx.frame, descriptor);
	}

	// dwt_context consumer_cx{
	// 	generate_dwt_id(), 
	// 	cx.channel_cx, 
	// 	cx.frame, 
	// 	descriptor
	// };
	// 
	// cx.channel_cx.descriptors.register_operation(consumer_cx);
	// return consumer_cx;
}

template <typename T>
template <typename oT>
void compression_routines<T>::postprocess_image(segmentation_context<subband_type, segment_type> cx) {
	auto op_state_token = cx.channel_cx.descriptors.start_operation(cx);

	auto& data = std::get<compression_data<T>>(cx.channel_cx.data);
	decltype(data.fragments) local_fragments;
	{
		std::lock_guard lock(data.fragments_mx);
		data.fragments.swap(local_fragments);
	}

	std::sort(local_fragments.begin(), local_fragments.end(),
		[](const auto& lhs_pair, const auto& rhs_pair) -> bool {
			return lhs_pair.first.y < rhs_pair.first.y;
		});

	bitmap<oT> result;
	for (auto&& item : local_fragments) {
		auto& fragment_data = io_data_registry::get_data<image_selector<oT>>(item.second);
		result.append(fragment_data.image);
		cx.channel_cx.session_cx.data_registry.free_descriptor(item.second);
	}

	image_memory_descriptor<oT> descriptor_data(std::move(result), cx.channel_cx.channel_index);

	const data_descriptor& descriptor =
		cx.channel_cx.session_cx.data_registry.put_output(cx.channel_cx.session_cx, std::move(descriptor_data));

	// TODO: drop padded rows and columns

	return;
}

// using decompression_parameters = std::tuple<
// 	std::vector<std::reference_wrapper<const data_descriptor>>, 
// 	size_t>;

#include <optional> // really need optional?
// decompression_parameters 
size_t collect_decompression_session_params(session_context& cx, std::vector<std::reference_wrapper<const data_descriptor>>& handles) {
	// handles are expected to be sorted by channel_id <| segment id
	using word_t = size_t;
	bool valid = true;

	std::optional<session_settings> session_params;
	std::optional<size_t> img_pad_row_count;
	decltype(session_context::compr_settings) compression_params;
	decltype(session_context::seg_settings) segment_params;

	size_t channel_start_count = 0;
	size_t channel_end_count = 0;
	size_t next_segment_id = segment_descriptor_base::id_unknown >> 1;	// TODO: proper constant? some kind of obviously non-zero value
	constexpr size_t segment_count_mask = 0x0100 - 1;	// TODO: magic, make interface constant
	for (auto item : handles) {
		auto segment_descriptor_parser = 
			[](const io_data_registry& registry, const data_descriptor& handle) -> segment_descriptor_base& {
				switch (registry.get_segment_storage_type()) {
				case storage_type::file: {
					return io_data_registry::get_data<segment_selector<storage_type::file>>(handle);
					break;
				}
				case storage_type::memory: {
					return io_data_registry::get_data<segment_selector<storage_type::memory>>(handle);
					break;
				}
				default:
					break;
				}
				throw ccsds::exception{};
			};

		segment_descriptor_base& descriptor = segment_descriptor_parser(cx.data_registry, item);
		auto source = make_source<word_t>(cx, item);	// TODO: need to pass context as parameter? 
		source->setup_session();
		// But legitimate use case is to check headers prior to context creation

		auto proto = ccsds_header_parser<ccsds_file_protocol>::parse_header(source->get_bitwrapper());
		if (proto.if_first()) {
			if (proto.if_contains_session_params()) {
				valid &= (descriptor.segment_id == 0);
				valid &= !session_params.has_value();
				if (!valid) {
					// TODO: error handling
				}
				session_params = proto.get_session_params();
			} // otherwise this is just another image channel of the same image session
			++channel_start_count;
			next_segment_id = 0;
		}
		if (proto.if_last()) {
			if (!img_pad_row_count.has_value()) {
				img_pad_row_count = proto.get_pad_rows_count();
			}
			++channel_end_count;
		}

		valid &= (proto.get_segment_count() == (next_segment_id & segment_count_mask));
		descriptor.segment_id = next_segment_id;
		descriptor.channel_id = channel_start_count - (channel_start_count > 0);	// TODO: really should bother about UB here?

		if (proto.if_contains_compression_params()) {
			// TODO: valid compressor *could* apply different settings set for different channels.
			// Major refactor would be needed to support this scenario.
			// Note applies to the corresponding logic below throughout the function, apply 
			// changes there as well when this issue being fixed.
			compression_params.push_back({ descriptor.segment_id, proto.get_compression_params() });
		}
		if (proto.if_contains_segment_params()) {
			segment_params.push_back({ descriptor.segment_id, proto.get_segment_params() });
		}

		++next_segment_id;
	}

	valid &= session_params.has_value();
	valid &= (img_pad_row_count.has_value() && (img_pad_row_count.value() < 8)); // TODO: magic numbers; pad_rows is guaranteed to be less than 8 by bitfield implementation
	valid &= !compression_params.empty();
	valid &= !segment_params.empty();
	valid &= (channel_start_count == channel_end_count);
	if (!valid) {
		// TODO: handle
	}

	size_t channel_count = channel_start_count;
	(*session_params).rows_pad_count = img_pad_row_count.value();

	auto validate_params_collection = [channel_count](auto& collection) -> void {
		std::sort(collection.begin(), collection.end(),
			[](const auto& lhs, const auto& rhs) -> bool {
				return lhs.first < rhs.first;
			});

		const auto* prev_item = &(collection.front());
		size_t same_count = 0;
		for (const auto& item : collection) {
			bool same_index = (item.first == prev_item->first);
			if (!same_index) {
				bool valid = true;
				valid &= (same_count == channel_count);
				if (!valid) {
					// TODO: handle error, throw
				}

				prev_item = &item;
				same_count = 0;
			}
			++same_count;
		}
		if (same_count != channel_count) {
			// TODO: handle error, throw
		}

		auto new_end_it = std::unique(collection.begin(), collection.end());
		collection.erase(new_end_it, collection.end());
	};

	validate_params_collection(compression_params);
	validate_params_collection(segment_params);

	cx.settings_session = *std::move(session_params);
	cx.compr_settings = std::move(compression_params);
	cx.seg_settings = std::move(segment_params);

	// std::move(handles);
	// return {
	// 	*std::move(session_params), 
	// 	std::move(compression_params), 
	// 	std::move(segment_params), 
	// 	channel_count
	// };
	// return { std::move(handles) , channel_count };
	return channel_count;
}

template <typename Derived>
struct session_parameters_parser_base {
	// parameters type (first template argument) must have get_session() that returns session_context&
	// Derived must have invoke_by_argument_type overload for used parameter type

protected:
	template <typename xbwT, dwt_type_t dwt_type, bool pixel_signed, typename ArgsT>
	static void parse_pixel_bdepth(ArgsT&& params) {
		const session_context& cx = params.get_session();

		auto& values = bdepth_implementation_catalog<dwt_type, pixel_signed>::values;

		bool valid = true;
		valid &= (cx.settings_session.pixel_bdepth < values.back());
		if (!valid) {
			// TODO: parsing error
		}

		size_t target_bdepth = *std::lower_bound(values.cbegin(), values.cend(), cx.settings_session.pixel_bdepth);

		auto dispatch_bdepth = [&]<size_t index>(size_t) -> void {
			constexpr size_t static_bdepth = bdepth_implementation_catalog<dwt_type, pixel_signed>::values[index];
			if (target_bdepth == static_bdepth) {
				using types = img_type_params<dwt_type, static_bdepth, pixel_signed>;
				using img_t = types::bitmap_type;
				using dwt_t = types::dwt_param_type;
				using xbw_t = xbwT;

				Derived::template invoke_by_argument_type<xbw_t, img_t, dwt_t>(std::move(params));
			}
		};

		std::apply([&](auto... values) -> void {
			unroll<>::apply(dispatch_bdepth, values...);
			},
			bdepth_implementation_catalog<dwt_type, pixel_signed>::values);
	}

	template <typename xbwT, dwt_type_t dwt_type, typename ArgsT>
	static void parse_pixel_signed(ArgsT&& params) {
		const session_context& cx = params.get_session();
		switch (cx.settings_session.signed_pixel) {
		case true: {
			parse_pixel_bdepth<xbwT, dwt_type, true>(std::move(params));
			break;
		}
		default: {
			parse_pixel_bdepth<xbwT, dwt_type, false>(std::move(params));
			break;
		}
		}
	}

	template <typename xbwT, typename ArgsT>
	static void parse_dwt_type(ArgsT&& params) {
		const session_context& cx = params.get_session();
		switch (cx.settings_session.dwt_type) {
		case dwt_type_t::idwt: {
			parse_pixel_signed<xbwT, dwt_type_t::idwt>(std::move(params));
			break;
		}
		case dwt_type_t::fdwt: {
			parse_pixel_signed<xbwT, dwt_type_t::fdwt>(std::move(params));
			break;
		}
		default: {
			// TODO: parsing error
		}
		}
	}

	template <typename ArgsT>
	static void parse_codeword_size(ArgsT&& params) {
		const session_context& cx = params.get_session();
		switch (cx.settings_session.codeword_size) {
		case 64: {	// TODO: or map to unsigned?
			parse_dwt_type<uint64_t>(std::move(params));
			break;
		}
		case 32: {
			parse_dwt_type<uint32_t>(std::move(params));
			break;
		}
		default: {
			constexpr size_t bindex_mask = (1 << 3) - 1;

			bool valid = true;
			valid &= ((cx.settings_session.codeword_size & bindex_mask) == 0);
			valid &= (cx.settings_session.codeword_size <= 64);
			if (!valid) {
				// TODO: parsing error
			}

			parse_dwt_type<uint8_t>(std::move(params));
			break;
		}
		}
	}
};