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
#include "io/sink.hpp"

#include "io/file_proto.hpp"

#include "dwt/dwt.hpp"
#include "dwt/segment_assembly.hpp"
#include "bpe/bpe.tpp"

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

	static segmentation_context<subband_type> preprocess_fragments(dwt_context cx);

	static std::vector<compression_context<segment_type>> assemble_segments(segmentation_context<subband_type> cx);

	static void compress_segment(compression_context<segment_type> data);

private:
	static std::vector<subbands_t<subband_type>> collect_contiguous_fragments(dwt_context cx);

	// TODO: merge_subbands uses context argument just to obtain image width value, it's better 
	// to pass image width explicitly
	static std::array<subbands_t<subband_type>, 2> merge_subbands(size_t image_width, std::vector<subbands_t<subband_type>>&& fragments);

	static std::vector<compression_context<segment_type>> dispatch_segments(channel_context& context, std::vector<std::unique_ptr<segment<segment_type>>>&& segments);

	// static void free_compressed_segment_data(channel_context& context);
};

template <typename T>
template <typename iT>
std::vector<dwt_context> compression_routines<T>::preprocess_image(dwt_context cx) {
	auto op_state_token = cx.channel_cx.register_operation_start(cx);
	auto& transformer = per_thread::compressors<T>::get_transformer(cx.channel_cx.session_cx);

	bitmap<iT>& img = std::get<std::vector<bitmap<iT>>>(cx.channel_cx.session_cx.image_channels)[cx.frame.z];

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
	}
	else {
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

		frames.emplace_back(generate_dwt_id(), cx.channel_cx, frame_item);

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

	cx.channel_cx.register_operations(frames);
	return frames;
}

template <typename T>
template <typename iT>
void compression_routines<T>::transform_fragment(dwt_context cx) {
	auto op_state_token = cx.channel_cx.register_operation_start(cx);
	auto& transformer = per_thread::compressors<T>::get_transformer(cx.channel_cx.session_cx);

	if (cx.channel_cx.session_cx.settings_session.custom_shifts) {
		transformer.get_scale().set_shifts(cx.channel_cx.session_cx.settings_session.shifts);
	}

	const bitmap<iT>& img = std::get<std::vector<bitmap<iT>>>(cx.channel_cx.session_cx.image_channels)[cx.frame.z];
	auto& data = std::get<compression_data<T>>(cx.channel_cx.data);

	auto result_subbands = transformer.apply(img, cx.frame);

	std::lock_guard lock(data.subband_fragments_mx);
	data.subband_fragments.emplace_back(std::move(result_subbands), cx.frame);
}

template <typename T>
segmentation_context<typename compression_routines<T>::subband_type> compression_routines<T>::preprocess_fragments(dwt_context cx) {
	auto op_state_token = cx.channel_cx.register_operation_start(cx);
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
	
	segmentation_context<subband_type> result {
		generate_segmentation_id(),
		cx.channel_cx,
		std::move(merged),
		std::make_unique<segment<segment_type>>(), 
		merged_frame
	};

	cx.channel_cx.register_operation(result);
	return result;
}

template <typename T>
std::vector<subbands_t<typename compression_routines<T>::subband_type>> compression_routines<T>::collect_contiguous_fragments(dwt_context cx) {
	auto& data = std::get<compression_data<T>>(cx.channel_cx.data);
	// using fragment_description_t = decltype(data.subband_fragments)::value_type;

	using view_iterator_t = decltype(data.subband_fragments)::const_iterator;
	std::vector<view_iterator_t> fragments_view;
	// (
	// std::vector<std::reference_wrapper<fragment_description_t>> fragments_view(
	// 	data.subband_fragments.cbegin(), data.subband_fragments.cend());

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

	// std::vector<subbands_t<subband_type>> result;
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
			// return (item->second.x < x) &
			// 	(item->second.y <= y) &
			// 	(item->second.z <= z);
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
			// result.push_back(std::move(item->first));
			
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
	//std::move(splice_buffer.begin(), splice_buffer.end(), std::back_inserter(result));
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

	// using subT = typename types::subband_type;

	// size_t image_width = context.settings_session.img_width;
	// image_width = ((image_width + 7) >> 3) << 3; // TODO: we should account for padding...
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

				do {
					item_frame.width = std::min(merged_subband_width - dst_frame.x, item_subband_width - item_frame.x);
					item_frame.x_step = item_frame.width;
					dst_frame.width = item_frame.width;
					dst_frame.x_step = item_frame.x_step;

					merged[subband_index].slice(dst_frame).assign(item[subband_index].slice(item_frame));

					item_frame.x += item_frame.x_step;
					dst_frame.x += dst_frame.x_step;

					bool right_edge_reached = (dst_frame.x >= merged_subband_width);
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
std::vector<compression_context<typename compression_routines<T>::segment_type>> compression_routines<T>::assemble_segments(segmentation_context<typename compression_routines<T>::subband_type> cx) {
	auto op_state_token = cx.channel_cx.register_operation_start(cx);
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
	cx.channel_cx.register_operations(result);
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

	// using segT = types::segment_type;
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
				make_sink<compression_context<segment_type>::sink_value_type>(context.session_cx, segment_id, context.session_cx.dst_type, context.channel_index),
				//std::make_unique<segment<segment_type>>(std::move(*it))
				std::move(*it)
			};
			// compression_descriptor segment_descriptor {
			// 	context_id,
			// 	operation_state::pending,
			// 	segment_id
			// };
			// 
			// // TODO: sync!
			// // context.compression_descriptors.push_back(std::move(segment_descriptor));
			// dispatched_descriptors.push_back(std::move(segment_descriptor));
			dispatched_segments.push_back(std::move(segment_context));
		}

		available_id_range.first += segment_num_to_dispatch;
		return it;
	};

	// {
	// 	size_t next_registered_segment_id = 0;
	// 	if (!dispatched_descriptors.empty()) {
	// 		next_registered_segment_id = dispatched_descriptors.front().segment_id;
	// 	} else {
	// 		// TODO: oops? Error handling?
	// 	}
	// 
	// 	std::lock_guard lock(context.compression_descriptors_mx);
	// 
	// 	[[unlikely]]
	// 	if (context.compression_descriptors.size() != next_registered_segment_id) {
	// 		// oops?
	// 	}
	// 
	// 	for (ptrdiff_t i = 0; i < dispatched_descriptors.size(); ++i) {
	// 		auto [it, inserted] = context.compression_segment_id_map.insert(
	// 			{ dispatched_descriptors[i].context_id, next_registered_segment_id + i });
	// 
	// 		[[unlikely]]
	// 		if (!inserted) {
	// 			// oops?
	// 		}
	// 	}
	// 
	// 	std::move(dispatched_descriptors.begin(), dispatched_descriptors.end(), 
	// 		std::back_inserter(context.compression_descriptors));
	// }
	context.register_operations(dispatched_segments);

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
		}
	}

	return dispatched_segments;
}

// template <typename T>
// void compression_routines<T>::free_compressed_segment_data(channel_context& context) {
// 	auto& data = std::get<compression_data<T>>(context.data);
// 	ptrdiff_t next_allocated_id = context.allocated_segment_id.first;
// 	size_t last_allocated_id = context.allocated_segment_id.second;
// 
// 	{
// 		std::shared_lock lock(context.compression_descriptors_mx);
// 		while ((next_allocated_id != last_allocated_id) && 
// 				(context.compression_descriptors[next_allocated_id].state.load() == operation_state::done)) {
// 			++next_allocated_id;
// 		}
// 	}
// 
// 	context.allocated_segment_id.first = next_allocated_id;
// }

template <typename T>
void compression_routines<T>::compress_segment(compression_context<typename compression_routines<T>::segment_type> cx) {
	auto op_state_token = cx.channel_cx.register_operation_start(cx);
	auto& encoder = per_thread::compressors<T>::get_encoder(cx.channel_cx.session_cx);
	auto [param_compr, flag_compr] = get_compression_settings(cx.channel_cx.session_cx, cx.segment_data->id);
	auto [param_seg, flag_seg] = get_segment_settings(cx.channel_cx.session_cx, cx.segment_data->id);

	encoder.set_use_heuristic_DC(param_seg.heuristic_quant_DC);
	encoder.set_use_heuristic_bdepthAc(param_seg.heuristic_bdepth_AC);
	if (param_compr.early_termination) {
		encoder.set_stop_after_DC(param_compr.DC_stop);
		encoder.set_stop_at_bplane(param_compr.bplane_stop, param_compr.stage_stop);
	}
	else {
		// refactor default params
		encoder.set_stop_after_DC(false);
		encoder.set_stop_at_bplane(0, 0b11);
	}

	// TODO: refactor somehow???
	ccsds_file_protocol specific_protocol(*(cx.segment_data), cx.segment_data->id);
	std::vector<std::byte> header_storage(specific_protocol.header_size());
	specific_protocol.set_compression_params(param_compr);
	specific_protocol.set_segment_params(param_seg);
	specific_protocol.commit(std::span(header_storage));
	// TODO: last/first?
	cx.dst->setup_session(param_seg, param_compr, flag_seg, flag_compr);

	try {
		ptrdiff_t header_offset = specific_protocol.ccsds_protocol::header_size() - specific_protocol.header_size();
		cx.dst->get_bitwrapper().set_byte_limit(param_compr.seg_byte_limit, header_offset);
		cx.dst->get_bitwrapper().write_bytes(std::span(header_storage));
		encoder.encode(*(cx.segment_data), cx.dst->get_bitwrapper());
		cx.dst->finish_session();
	}
	catch (const ccsds::bpe::byte_limit_exception& ex) {
		cx.dst->handle_early_termination();
	}

	specific_protocol.set_segment_size(cx.dst->get_bitwrapper().get_byte_count());
	// TODO: override protocol header?
	
	// TODO: context descriptor should contain info about compression result, some kind of 
	// sink handle that can be used to access the result.
}


// template <typename T>
// std::vector<compression_context<typename compression_routines<T>::segment_type>> compression_routines<T>::dispatch_segments(const session_context& context, std::vector<std::unique_ptr<segment<typename compression_routines<T>::segment_type>>>&& segments) {
// 	// handle segment preprocessing work: allocate sink, make necessary sink configs
// 	// 
// 	free_compressed_segment_data(context);
// 
// 	size_t next_id = context.allocated_segment_id.second + 1;
// 	size_t next_allocated_id = context.allocated_segment_id.first;
// 
// 	next_id &= 255;
// 	bool id_available = (next_id != next_allocated_id);
// 
// 	bool range_wrap = next_id > next_allocated_id;
// 	next_allocated_id += zeropred(256, range_wrap);
// 	size_t available_id_range_length = next_allocated_id - next_id;
// 
// 	auto& data = std::get<session_context::compression_data<T>>(context.compr_data);
// 
// 	// using segT = types::segment_type;
// 	std::vector<std::reference_wrapper<compression_context<segment_type>>> dispatched_segments;
// 
// 	std::array<std::reference_wrapper<decltype(segments)>, 2> segment_sets{ data.tail, segments };
// 	for (decltype(segments)& item : segment_sets) {
// 		size_t segment_num_to_dispatch = std::min(available_id_range_length, item.size());
// 
// 		for (ptrdiff_t i = 0; i < segment_num_to_dispatch; ++i) {
// 			ptrdiff_t context_index = (next_id + i) & 255;
// 			data.compression_contexts[context_index] = compression_context<segment_type>{
// 				make_sink<compression_context<segment_type>::sink_data_type>(context, next_id + i, context.dst_type),
// 				std::move(item[i]), context_index
// 			};
// 			dispatched_segments.push_back(data.compression_contexts[context_index]);
// 		}
// 		available_id_range_length -= segment_num_to_dispatch;
// 		next_id += segment_num_to_dispatch;
// 
// 		item.erase(item.begin(), std::advance(item.begin(), segment_num_to_dispatch));
// 	}
// 	std::move(segments.begin(), segments.end(), std::back_inserter(data.tail));
// 
// 	context.allocated_segment_id.second = next_id;
// 
// 	return dispatched_segments;
// }