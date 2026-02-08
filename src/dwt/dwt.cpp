#include "dwt.tpp"

#include <utility>
#include <variant>

#include "utils.hpp"

// non-member function declarations

namespace {
	enum class dwt_type {
		highpass,
		lowpass, 
		either,
		none // for second dimension if 1-d only
	};

	struct row_range {
		ptrdiff_t offset = 0;
		ptrdiff_t size = 0;
	};

	template <typename T, size_t alignment>
	void copy_lowpass_data(const bitmap<T, alignment>& src, bitmap<T, alignment>& dst, std::array<row_range, 2> ranges);
}

// ForwardWaveletTransformer class template implementation

template <typename T, size_t alignment>
union ForwardWaveletTransformer<T, alignment>::frame_overlap_description {
	std::array<size_t, 4> arr = {};
	struct {
		size_t top = 0;
		size_t left = 0;
		size_t right = 0;
		size_t bottom = 0;
	} edges;
};

template <typename T, size_t alignment>
ForwardWaveletTransformer<T, alignment>::ForwardWaveletTransformer() {
	this->buffers.fill(bitmap<T>(dwt::fwd_buffers_offset_value));
}

template <typename T, size_t alignment>
template <typename iT>
void ForwardWaveletTransformer<T, alignment>::preprocess_image(bitmap<iT>& src) {
	// This function is needed to eliminate concurrent memory writes from different 
	// transformers from different threads to the same regions of the input image. 
	// Such writes could be caused by extension or padding of image rows in image 
	// regions being overlapped by different fragments.
	// In case of fragmented image processing, the user should call this function 
	// in one thread only, before other threads begin processing.
	// 

	img_meta src_dims = src.get_meta();
	if (src_dims.width < constants::img::min_width) {
		// TODO: validation error?
		// if width is 0, UB is possible with unsigned underflow
	}

	size_t padded_width = (src_dims.width + (dwt::horizontal_padding_length - 1)) 
		& (~dwt::horizontal_padding_mask);

	for (ptrdiff_t i = 0; i < src_dims.height; ++i) {
		this->core.extfwd(src[i].ptr());
	}
	for (ptrdiff_t i = 0; i < src_dims.height; ++i) {
		bitmap_row<iT> src_row = src[i];
		T last_item = src_row[src_dims.width - 1];
		for (ptrdiff_t j = src_dims.width; j < padded_width; ++j) {
			src_row[j] = last_item;
		}

		this->core.rextfwd(src_row.ptr() + padded_width);
	}
	// this only handles horizontal padding and border extension. Vertical padding and 
	// extension should be handled on dwt coefficients intermediate data.
}

template <typename T, size_t alignment>
template <typename iT>
subbands_t<T> ForwardWaveletTransformer<T, alignment>::apply(const bitmap<iT>& source, const img_pos& frame) {
	img_meta src_dims = source.get_meta();
	// image dimensions adjusted for padding
	src_dims.width = (src_dims.width + (dwt::horizontal_padding_length - 1)) & (~dwt::horizontal_padding_mask);
	src_dims.height = (src_dims.height + (dwt::vertical_padding_length - 1)) & (~dwt::vertical_padding_mask);
	
	// TODO: or just make sure that padding changes observable dimensions of the image?
	bool valid = true;
	valid &= frame.width <= source.get_meta().width;

	if (!valid) {
		// TODO: validation error
		// frames with width greater than source image width are not supported. 
		// The user should set frame width equal to image width and increase 
		// frame height instead.
	}

	auto frames_decomposed = this->decompose_frame(src_dims, frame);
	bool double_frame = frames_decomposed[1].width > 0;


	std::array<frame_overlap_description, 2> frame_edge_overlap = { {
		{
			std::min(frames_decomposed[0].y, dwt::overlap_img_range),
			std::min(frames_decomposed[0].x, dwt::overlap_img_range),
			std::min(src_dims.width - (frames_decomposed[0].x + frames_decomposed[0].width), dwt::overlap_img_range),
			std::min(src_dims.height - (frames_decomposed[0].y + frames_decomposed[0].height), dwt::overlap_img_range)
		},
		{
			std::min(frames_decomposed[1].y, dwt::overlap_img_range),
			0,
			std::min(src_dims.width - (frames_decomposed[1].x + frames_decomposed[1].width), dwt::overlap_img_range),
			std::min(src_dims.height - (frames_decomposed[1].y + frames_decomposed[1].height), dwt::overlap_img_range)
		}
	} };
	{
		auto& tail_frame = frame_edge_overlap[1];
		// zeropred(tail_frame.top, double_frame);
		// zeropred(tail_frame.left, double_frame);
		// zeropred(tail_frame.right, double_frame);
		// zeropred(tail_frame.bottom, double_frame);
		for (auto& item : tail_frame.arr) {
			item = zeropred(item, double_frame);
		}
	}

	// buffer resize and memory allocation staff
	{
		size_t base_width = frames_decomposed[0].width + frames_decomposed[1].width;
		size_t base_height = frames_decomposed[0].height; // the second frame's height is no bigger
		std::array<frame_overlap_description, 2> current_overlap = frame_edge_overlap;

		this->resize_buffers(base_width, base_height, current_overlap);
	}

	// skip unnecessary coefficient data during transpose step after dwt application: 
	//	read rows of result coefficients unaligned (with positive offset equal to filter 
	//	dependency range size, i.e. +4 for lowpass and +3 for highpass).
	//		for every LL subband positive offset is same due to hierarchical subband structure. 
	//		for other subbands offset depends on filter type and level index.
	//	this way we can eliminate excessive storage allocations!
	// 
	// buffer sizes can be tuned accordingly.
	//

	// vertical padding:
	// what if the image itself is perfectly sliced into fragments' height (with unpadded 
	// dimensions), but padding creates a region at the bottom that doesn't fit in the last 
	// layer fragment and necessiates additional row of fragments (with padded data only)?
	//	this should not be possible: padding itself only ensures that shape of level buffers 
	//	correspond to input image shape. If there's unfit level 3 coefficient, then at least 
	//	8 rows of image is left for the next fragment row.
	//

	struct scan_traverse_tracker {
		ptrdiff_t x = 0;
		ptrdiff_t x_step = 0;
	};

	std::array<scan_traverse_tracker, 15> frame_positions;

	ptrdiff_t max_frame_index = frames_decomposed.size() - (!double_frame);
	for (ptrdiff_t frame_index = 0; frame_index < max_frame_index; ++frame_index) {

		img_pos src_frame = frames_decomposed[frame_index];
		src_frame.x -= frame_edge_overlap[frame_index].edges.left;
		src_frame.width += frame_edge_overlap[frame_index].edges.left + frame_edge_overlap[frame_index].edges.right;
		src_frame.y -= frame_edge_overlap[frame_index].edges.top;
		src_frame.height += frame_edge_overlap[frame_index].edges.top + frame_edge_overlap[frame_index].edges.bottom;
		// vertical padding for source is not applied yet. Adjust src frame accordingly
		size_t height_limit = source.get_meta().height - src_frame.y;
		src_frame.height = std::min(src_frame.height, source.get_meta().height - src_frame.y);

		constexpr size_t src_slice_index = 0;
		constexpr size_t buffer_slice_index = 1;
		std::variant<const_bitmap_slice<iT>, const_bitmap_slice<T>> src{ std::in_place_index<src_slice_index>, source.slice(src_frame) };
		// const_bitmap_slice<T> src = source.slice(src_frame);

		ptrdiff_t overlap_requirement = dwt::overlap_img_range;
		auto current_overlap = frame_edge_overlap[frame_index];
		size_t level_frame_width = frames_decomposed[frame_index].width;
		size_t level_frame_height = frames_decomposed[frame_index].height;

		auto make_frame = [&]<dwt_type horizontal, dwt_type vertical = dwt_type::none>() -> img_pos {
			static_assert(horizontal != dwt_type::none);
			img_pos result{};
			result.width = level_frame_width;
			result.height = level_frame_height;

			if constexpr (horizontal == dwt_type::lowpass) {
				result.width += current_overlap.edges.left + current_overlap.edges.right;
				if constexpr (vertical != dwt_type::highpass) {
					result.height += current_overlap.edges.top + current_overlap.edges.bottom;
				}
			}
			else if constexpr (vertical == dwt_type::none) {
				result.height += current_overlap.edges.top + current_overlap.edges.bottom;
			}
			//		width	height
			// l	+		+
			// ll	+		+
			// lh	+		-
			// h	-		+
			// hl	-		-
			// hh	-		-
			//
			
			return result;
		};

		bool src_is_image_data = true;
		bool& apply_height_padding = src_is_image_data;
		bool& skip_src_extension = src_is_image_data;
		for (ptrdiff_t level = 0; level < dwt::level_num; ++level) {
			overlap_requirement -= dwt::lowpass_dependency_radius;

			ptrdiff_t hout_drop_offset = current_overlap.edges.left >> 1;
			ptrdiff_t lout_drop_offset = relu(current_overlap.edges.left - overlap_requirement) >> 1; // TODO: check integer conversion here
			ptrdiff_t llout_drop_offset = relu(current_overlap.edges.top - overlap_requirement) >> 1; // TODO: check integer conversion here
			ptrdiff_t hxout_drop_offset = current_overlap.edges.top >> 1;
			ptrdiff_t lhout_drop_offset = current_overlap.edges.top >> 1;
			// but lh buffer will contain vertical overlap remainders after these dropout settings

			// bet for effective conditional move
			current_overlap.edges.left = (current_overlap.edges.left > overlap_requirement) ? (current_overlap.edges.left - dwt::lowpass_dependency_radius) : current_overlap.edges.left;
			current_overlap.edges.right = (current_overlap.edges.right > overlap_requirement) ? (current_overlap.edges.right - dwt::lowpass_dependency_radius) : current_overlap.edges.right;
			current_overlap.edges.left = relu(current_overlap.edges.left);
			current_overlap.edges.right = relu(current_overlap.edges.right);
			current_overlap.edges.left >>= 1;
			current_overlap.edges.right >>= 1;
			level_frame_width >>= 1;

			ptrdiff_t hout_index = level * 3 + 0;
			img_pos hout_frame = make_frame.template operator()<dwt_type::highpass>();
			hout_frame.x = frame_positions[hout_index].x;
			frame_positions[hout_index].x_step = hout_frame.width;
			bitmap_slice<T> hout = this->buffers[hout_index].slice(hout_frame.transpose());
			
			ptrdiff_t lout_index = level * 3 + 1;
			img_pos lout_frame = make_frame.template operator()<dwt_type::lowpass>();
			lout_frame.x = frame_positions[lout_index].x;
			frame_positions[lout_index].x_step = lout_frame.width;
			bitmap_slice<T> lout = this->buffers[lout_index].slice(lout_frame.transpose());

			current_overlap.edges.top = (current_overlap.edges.top > overlap_requirement) ? (current_overlap.edges.top - dwt::lowpass_dependency_radius) : current_overlap.edges.top;
			current_overlap.edges.bottom = (current_overlap.edges.bottom > overlap_requirement) ? (current_overlap.edges.bottom - dwt::lowpass_dependency_radius) : current_overlap.edges.bottom;
			current_overlap.edges.top = relu(current_overlap.edges.top);
			current_overlap.edges.bottom = relu(current_overlap.edges.bottom);
			current_overlap.edges.top >>= 1;
			current_overlap.edges.bottom >>= 1;
			level_frame_height >>= 1;
			overlap_requirement >>= 1;

			ptrdiff_t llout_index = level * 3 + 2;
			img_pos llout_frame = make_frame.template operator()<dwt_type::lowpass, dwt_type::lowpass>();
			llout_frame.x = frame_positions[llout_index].x;
			llout_frame.x = (frame_index == 0) ? llout_frame.x : 
				((llout_frame.x + dwt::overlap_crossrow_buffer_width + (alignment - 1)) & (~(alignment - 1)));
			frame_positions[llout_index].x_step = llout_frame.width;
			bitmap_slice<T> llout = this->buffers[llout_index].slice(llout_frame);

			// frame dimensions and positions are the same for horizontal highpass buffers
			img_pos hxout_frame = make_frame.template operator()<dwt_type::highpass, dwt_type::either>();
			auto& hxout_positions = frame_positions[frame_positions.size() - (level + 1) * 2 + 0];
			hxout_frame.x = hxout_positions.x;
			hxout_positions.x_step = hxout_frame.width;

			bitmap_slice<T> hhout = this->buffers[this->buffers.size() - (level + 1) * 3 + 2].slice(hxout_frame);
			bitmap_slice<T> hlout = this->buffers[this->buffers.size() - (level + 1) * 3 + 0].slice(hxout_frame);

			auto& lhout_positions = frame_positions[frame_positions.size() - (level + 1) * 2 + 1];
			img_pos lhout_frame = make_frame.template operator()<dwt_type::lowpass, dwt_type::highpass>();
			lhout_frame.x = lhout_positions.x;
			lhout_positions.x_step = lhout_frame.width;
			bitmap_slice<T> lhout = this->buffers[this->buffers.size() - (level + 1) * 3 + 1].slice(lhout_frame);

			// Main chain
			// this->transform(src, hout, lout, hout_drop_offset, lout_drop_offset, skip_src_extension);
			if (src_is_image_data) {
				auto& src_slice = std::get<src_slice_index>(src);
				this->transform(src_slice, hout, lout, hout_drop_offset, lout_drop_offset, skip_src_extension);
			} else {
				auto& src_slice = std::get<buffer_slice_index>(src);
				this->transform(src_slice, hout, lout, hout_drop_offset, lout_drop_offset, skip_src_extension);
			}
			
			if (apply_height_padding) {
				// apply vertical padding in intermediate level 1 buffers
				size_t unpadded_source_height = source.get_meta().height;
				auto& current_img_frame = frames_decomposed[frame_index];
				auto& current_img_overlap = frame_edge_overlap[frame_index];
				if ((current_img_frame.y + current_img_frame.height + current_img_overlap.edges.bottom) > 
						unpadded_source_height) {
					std::array<std::reference_wrapper<bitmap_slice<T>>, 2> buffers_to_pad{ hout, lout };
					for (auto& buf_ref : buffers_to_pad) {
						bitmap_slice<T>& buf = buf_ref;
						img_pos buf_dims = buf.get_description();
						ptrdiff_t padding_start_index = buf_dims.width - (src_dims.height - unpadded_source_height);
						for (ptrdiff_t i = 0; i < buf_dims.height; ++i) {
							bitmap_row<T> row = buf[i];
							T last_item = row[padding_start_index - 1];
							for (ptrdiff_t j = padding_start_index; j < buf_dims.width; ++j) {
								row[j] = last_item;
							}
						}
					}
				}
			}

			this->transform(static_cast<const_bitmap_slice<T>>(lout), lhout, llout, lhout_drop_offset, llout_drop_offset);

			// Aux chain
			this->transform(static_cast<const_bitmap_slice<T>>(hout), hhout, hlout, hxout_drop_offset, hxout_drop_offset);

			// src = static_cast<const_bitmap_slice<T>>(llout);
			src.emplace<buffer_slice_index>(static_cast<const_bitmap_slice<T>>(llout));
			src_is_image_data = false;
		}

		for (auto& item : frame_positions) {
			item.x += item.x_step;
		}
	}

	subbands_t<T> result; // TODO: check type match, maybe alignment parameter needed
	result.fill(bitmap<T>(dwt::fwd_buffers_offset_value));

	ptrdiff_t overlap_requirement = dwt::overlap_img_range;
	// buffers are allocated in the order below in return value:
	// LL3, HL3, LH3, HH3, HL2, LH2, HH2, HL1, LH1, HH1
	for (ptrdiff_t level = 0; level < dwt::level_num; ++level) {
		overlap_requirement -= dwt::lowpass_dependency_radius;
		for (auto& overlap_item : frame_edge_overlap) {
			overlap_item.edges.left  = (overlap_item.edges.left  > overlap_requirement) ? 
				(overlap_item.edges.left  - dwt::lowpass_dependency_radius) : overlap_item.edges.left;
			overlap_item.edges.right = (overlap_item.edges.right > overlap_requirement) ? 
				(overlap_item.edges.right - dwt::lowpass_dependency_radius) : overlap_item.edges.right;

			overlap_item.edges.left = relu(overlap_item.edges.left);
			overlap_item.edges.right  = relu(overlap_item.edges.right);

			overlap_item.edges.left >>= 1;
			overlap_item.edges.right >>= 1;
		}
		overlap_requirement >>= 1;;

		for (auto& frame_item : frames_decomposed) {
			frame_item.width >>= 1;
			frame_item.height >>= 1;
		}

		for (ptrdiff_t i = 0; i < constants::subband::subbands_per_generation; ++i) {
			ptrdiff_t buffer_index = result.size() - (level + 1) * 3 + i;
			if ((i & 0x01) == 0) {
				using std::swap;
				swap(this->buffers[dwt::LL3_index + buffer_index], result[buffer_index]);
			}
			else {
				std::array<row_range, 2> source_ranges = { {
					{
						frame_edge_overlap[0].edges.left,
						frames_decomposed[0].width
					}, 
					{
						frame_edge_overlap[0].edges.right + frame_edge_overlap[1].edges.left,
						frames_decomposed[1].width
					}
				} };
				img_meta target_dimensions = result[buffer_index - 1].get_meta();
				result[buffer_index].resize(target_dimensions.width, target_dimensions.height);

				copy_lowpass_data(this->buffers[dwt::LL3_index + buffer_index], result[buffer_index], source_ranges);

			}
			this->scale.scale(result[buffer_index], buffer_index);
		}
	}
	{
		constexpr ptrdiff_t result_LL3_index = 0;
		std::array<row_range, 2> source_ranges = { {
			{
				0, // all overlap items are dropped already for LL3
				frames_decomposed[0].width
			}, 
			{
				((frames_decomposed[0].width + dwt::overlap_crossrow_buffer_width + (alignment - 1)) & (~(alignment - 1))) - frames_decomposed[0].width,
				frames_decomposed[1].width
			}
		} };
		img_meta target_dimensions = result[1].get_meta();
		result[result_LL3_index].resize(target_dimensions.width, target_dimensions.height);

		copy_lowpass_data(this->buffers[dwt::LL3_index], result[result_LL3_index], source_ranges);
		this->scale.scale(result[result_LL3_index], result_LL3_index);
	}

	return result;
}

template <typename T, size_t alignment>
template <typename iT>
subbands_t<T> ForwardWaveletTransformer<T, alignment>::apply(bitmap<iT>& source) {
	this->preprocess_image(source);

	img_pos img_frame = source.single_frame_params();
	img_frame.width = (img_frame.width + (dwt::horizontal_padding_length - 1)) & (~dwt::horizontal_padding_mask);
	img_frame.height = (img_frame.height + (dwt::vertical_padding_length - 1)) & (~dwt::vertical_padding_mask);

	return this->apply(source, img_frame);
}

template <typename T, size_t alignment>
dwtscale<T>& ForwardWaveletTransformer<T, alignment>::get_scale() {
	return this->scale;
}

template <typename T, size_t alignment>
template <typename iT>
void ForwardWaveletTransformer<T, alignment>::transform(const_bitmap_slice<iT> source, bitmap_slice<T> hdst, bitmap_slice<T> ldst,
		ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension) {

	// performance assumptions: 
	//		hdst and ldst are properly aligned
	//		source may be not aligned
	//		each input buffer has sufficient allocated area after the last row
	//

	constexpr size_t buffer_alignment = 32;
	static_assert(dwt::overlap_img_range < buffer_alignment);

	// TODO: or just skip validation checks?
	bool valid = true;
	valid &= hdst_drop_offset < dwt::overlap_img_range;
	valid &= ldst_drop_offset < dwt::overlap_img_range;
	if (!valid) {
		// TODO: validation error
	}

	std::array<std::array<T, alignment>, alignment> transpose_buffer = {};

	size_t buffer_width = source.get_description().width / 2;
	bitmap<T> lbuffer(buffer_width, alignment, buffer_alignment);
	bitmap<T> hbuffer(buffer_width, alignment, buffer_alignment);

	// size_t buffers_copy_width = std::max(hdst.get_description().height, ldst.get_description().height);
	// // and assume that tail values from the lesser buffer can be safely read from stride area

	for (ptrdiff_t i = 0; i < source.get_description().height; i += alignment) {
		ptrdiff_t max_src_row_index = std::min<ptrdiff_t>(source.get_description().height - i, alignment);
		for (ptrdiff_t j = 0; j < max_src_row_index; ++j) {
			bitmap_row<iT> source_row(source[i + j]);
			if (!skip_extension) {
				this->core.extfwd(source_row.ptr());
				this->core.rextfwd(source_row.ptr() + source_row.width());
			}
			this->core.fwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr(), buffer_width);
			this->core.corrhfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
			this->core.corrlfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
		}

		// debug block, inverse op
		if constexpr (dbg::dwt::encoder::if_enabled(dbg::dwt::mask_reverse_data)) {
			bitmap<T> dstbuffer(source.get_description().width, alignment, 32);
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				bitmap_row<T> hrow(hbuffer[j]);
				bitmap_row<T> lrow(lbuffer[j]);
				this->core.exthbwd(hrow.ptr());
				this->core.rexthbwd(hrow.ptr() + hrow.width());
				this->core.extlbwd(lrow.ptr());
				this->core.rextlbwd(lrow.ptr() + lrow.width());
				this->core.bwd(hrow.ptr(), lrow.ptr(), dstbuffer[j].ptr(), buffer_width);
			}

			for (ptrdiff_t ii = 0; ii < alignment; ++ii) {
				for (ptrdiff_t jj = 0; jj < source.get_description().width; ++jj) {
					if (source[i + ii][jj] != dstbuffer[ii][jj]) {
						throw "NEQ!";
					}
				}
			}
		}

		for (ptrdiff_t j = 0; j < hdst.get_description().height; j += alignment) {
			ptrdiff_t max_dst_row_index = std::min<ptrdiff_t>(hdst.get_description().height - j, alignment);
			for (ptrdiff_t ii = 0; ii < alignment; ++ii) {
				for (ptrdiff_t jj = 0; jj < alignment; ++jj) {
					transpose_buffer[jj][ii] = hbuffer[ii][j + jj + hdst_drop_offset];
				}
			}
			for (ptrdiff_t k = 0; k < max_dst_row_index; ++k) {
				bitmap_row<T> dstrow = hdst[j + k];
				for (ptrdiff_t l = 0; l < alignment; ++l) {
					dstrow[i + l] = transpose_buffer[k][l];
				}
			}
		}

		for (ptrdiff_t j = 0; j < ldst.get_description().height; j += alignment) {
			ptrdiff_t max_dst_row_index = std::min<ptrdiff_t>(ldst.get_description().height - j, alignment);
			for (ptrdiff_t ii = 0; ii < alignment; ++ii) {
				for (ptrdiff_t jj = 0; jj < alignment; ++jj) {
					transpose_buffer[jj][ii] = lbuffer[ii][j + jj + ldst_drop_offset];
				}
			}
			for (ptrdiff_t k = 0; k < max_dst_row_index; ++k) {
				bitmap_row<T> dstrow = ldst[j + k];
				for (ptrdiff_t l = 0; l < alignment; ++l) {
					dstrow[i + l] = transpose_buffer[k][l];
				}
			}
		}
	}
}

template <typename T, size_t alignment>
void ForwardWaveletTransformer<T, alignment>::set_target_frame_size(size_t frame_width, size_t frame_height) {
	frame_width = (frame_width + (dwt::horizontal_padding_length - 1)) & (~dwt::horizontal_padding_mask);
	frame_height = ((frame_height + (dwt::vertical_padding_length - 1)) & (~dwt::vertical_padding_mask));

	// prepare for maximum overlap for every edge for every frame. This may be 
	// slightly redundant for the tail frame, but simple enough and may lighten
	// memory pressure a bit
	//
	std::array<frame_overlap_description, 2> frame_edge_overlap;
	for (auto& item : frame_edge_overlap) {
		item.arr.fill(dwt::overlap_img_range);
	}

	this->resize_buffers(frame_width, frame_height, frame_edge_overlap);
}

template <typename T, size_t alignment>
std::array<img_pos, 2> ForwardWaveletTransformer<T, alignment>::decompose_frame(img_meta src_dims, img_pos target_frame) {
	bool params_valid = true;
	params_valid &= (target_frame.width & dwt::horizontal_padding_mask) == 0;
	params_valid &= (target_frame.height & dwt::vertical_padding_mask) == 0;
	params_valid &= (target_frame.x & dwt::horizontal_padding_mask) == 0;
	params_valid &= (target_frame.y & dwt::vertical_padding_mask) == 0;
	// TODO: input image dimensions are validated in high-level logic?

	params_valid &= target_frame.x < src_dims.width;
	params_valid &= target_frame.y < src_dims.height;
	params_valid &= target_frame.width <= src_dims.width;
	params_valid &= target_frame.height <= src_dims.height;
	params_valid &= target_frame.width != 0;
	params_valid &= target_frame.height != 0;
	params_valid &= target_frame.x_stride == src_dims.stride; // no support for non-rectangular 
		// frames, due to the nature of image edge handling.
	if (!params_valid) {
		// TODO: validation error
	}

	bool frame_divided = (src_dims.width < (target_frame.x + target_frame.width)) & 
		(src_dims.height > (target_frame.y + target_frame.height));

	std::array<img_pos, 2> result = { 
		target_frame, 
		target_frame
	};

	result[0].width = std::min(target_frame.width, src_dims.width - result[0].x);
	result[0].height = std::min(target_frame.height, src_dims.height - result[0].y);
	
	result[1].x = 0;
	result[1].y = result[0].y + result[0].height;
	result[1].width = zeropred(target_frame.width - result[0].width, frame_divided);
	result[1].height = zeropred(std::min(target_frame.height, src_dims.height - result[1].y), frame_divided);

	return result;
}

template <typename T, size_t alignment>
void ForwardWaveletTransformer<T, alignment>::resize_buffers(size_t base_width, size_t base_height, std::array<frame_overlap_description, 2>& overlap) {
	bool valid = true;
	valid &= (base_width & dwt::horizontal_padding_mask) == 0;
	valid &= (base_height & dwt::vertical_padding_mask) == 0;

	if (!valid) {
		// TODO: validation error
	}

	auto get_buffer_dimensions = [&]<dwt_type horizontal, dwt_type vertical = dwt_type::none>() -> std::pair<size_t, size_t> {
		std::pair<size_t, size_t> result = { base_width, base_height };

		if constexpr (horizontal == dwt_type::lowpass) {
			result.first += overlap[0].edges.left + overlap[0].edges.right;
			result.first += overlap[1].edges.left + overlap[1].edges.right;
			if constexpr (vertical != dwt_type::highpass) {
				result.second += overlap[0].edges.top + overlap[0].edges.bottom;
				result.second += overlap[1].edges.top + overlap[1].edges.bottom;
			}
		} else if constexpr (vertical == dwt_type::none) {
			result.second += overlap[0].edges.top + overlap[0].edges.bottom;
			result.second += overlap[1].edges.top + overlap[1].edges.bottom;
		}
		//		width	height
		// l	+		+
		// ll	+		+
		// lh	+		-
		// h	-		+
		// hl	-		-
		// hh	-		-
		//
		return result;
	};

	ptrdiff_t overlap_requirement = dwt::overlap_img_range;

	// H1, L1, LL1, H2, L2, LL2, H3, L3, LL3, HL3, LH3, HH3, HL2, LH2, HH2, HL1, LH1, HH1
	for (ptrdiff_t level = 0; level < dwt::level_num; ++level) {
		overlap_requirement -= dwt::lowpass_dependency_radius;
		for (auto& overlap_item : overlap) {
			for (auto& edge_item : overlap_item.arr) {
				edge_item = (edge_item > overlap_requirement) ? (edge_item - dwt::lowpass_dependency_radius) : edge_item;
				edge_item = relu(edge_item); // for source overlap = 8, by level 3 unsigned overflow happens otherwise
			}
			overlap_item.edges.left >>= 1;
			overlap_item.edges.right >>= 1;
		}
		base_width >>= 1;
		overlap_requirement >>= 1;

		{
			auto [height, width] = get_buffer_dimensions.template operator()<dwt_type::highpass>(); // transposed
			this->buffers[3 * level + 0].resize(width, height);
		}

		{
			auto [height, width] = get_buffer_dimensions.template operator()<dwt_type::lowpass>(); // transposed
			this->buffers[3 * level + 1].resize(width, height);
		}

		for (auto& overlap_item : overlap) {
			overlap_item.edges.top >>= 1;
			overlap_item.edges.bottom >>= 1;
		}
		base_height >>= 1;

		{
			auto [width, height] = get_buffer_dimensions.template operator()<dwt_type::lowpass, dwt_type::lowpass>();
			
			// make sure there's enough room for complete alignment between frame.
			// It's not possible to compute precise alignment adjustment without 
			// specific frame dimensions (values vary depending on frames' x's and 
			// widths)
			// 
			// line below won't work:
			// width = (width + (alignment - 1)) & (~(alignment - 1)) + alignment; 
			width += alignment; 
			width += dwt::overlap_crossrow_buffer_width;
			this->buffers[3 * level + 2].resize(width, height);
		}

		{
			auto [width, height] = get_buffer_dimensions.template operator()<dwt_type::highpass, dwt_type::either>();
			this->buffers[18 - ((level + 1) * 3) + 0].resize(width, height);
			this->buffers[18 - ((level + 1) * 3) + 2].resize(width, height);
		}

		{
			auto [width, height] = get_buffer_dimensions.template operator()<dwt_type::lowpass, dwt_type::highpass>();
			this->buffers[18 - ((level + 1) * 3) + 1].resize(width, height);
		}
	}
}


// BackwardWaveletTransformer class template implementation

template <typename T, size_t alignment>
BackwardWaveletTransformer<T, alignment>::BackwardWaveletTransformer() {
	this->buffers.fill(bitmap<T>(dwt::bwd_buffers_offset_value));
}

template <typename T, size_t alignment>
template <typename oT>
bitmap<oT> BackwardWaveletTransformer<T, alignment>::apply(subbands_t<T>& subbands, size_t top_overlap) {
	img_meta dst_img_dims = subbands.front().get_meta(); // based on LL3 dimensions
	this->resize_buffers(dst_img_dims.width, dst_img_dims.height, top_overlap);

	dst_img_dims.width <<= 3;
	dst_img_dims.height -= top_overlap;
	dst_img_dims.height <<= 3;
	// dst is meant to be transposed at the end of backward transform
	bitmap<T> dst(dst_img_dims.height, dst_img_dims.width, dwt::bwd_buffers_offset_value);

	auto preprocess_subband = [&](ptrdiff_t subband_index, size_t rows_drop_offset = 0, bool skip_unscale = false) 
			-> bitmap<T> {
		const bitmap<T>& src_subband = subbands[subband_index];
		img_pos frame = src_subband.single_frame_params();
		frame.y += rows_drop_offset;
		frame.height -= rows_drop_offset;

		// would be better to enforce URVO, but unscale returns void
		bitmap<T> result = src_subband.slice(frame).transpose();
		if (!skip_unscale) {
			this->scale.unscale(result, subband_index);
		}
		return result;
	};

	// size_t buffers_overlap = top_overlap;
	size_t buffers_overlap = std::min(constants::dwt::lowpass_dependency_radius, top_overlap);
	size_t subband_overlap = top_overlap;
	size_t subband_rows_offset = subband_overlap - buffers_overlap;

	bitmap<T> LL3_transposed = preprocess_subband(0, subband_rows_offset, this->skip_dc_scaling);
	// subbands.front().transpose();
	// if (!this->skip_dc_scaling) {
	// 	this->scale.unscale(LL3_transposed, 0);
	// }

	constexpr ptrdiff_t LL2_index = 5;
	constexpr ptrdiff_t LL1_index = 2;
	std::array<std::reference_wrapper<bitmap<T>>, 4> ll_collection {
		dst, this->buffers[LL1_index], this->buffers[LL2_index], LL3_transposed
	};

	for (ptrdiff_t level = constants::dwt::level_num; level > 0; --level) {
		subband_rows_offset = subband_overlap - buffers_overlap;

		size_t disturbed_top_rows_num = zeropred(constants::dwt::lowpass_dependency_radius, 
			buffers_overlap >= constants::dwt::lowpass_dependency_radius);

		// accounts not for disturbed rows really, but uses drop counter to drop overlay remainder 
		// for the target image on the last transform level
		disturbed_top_rows_num += zeropred(buffers_overlap, level == 1);

		bitmap<T>& hout = this->buffers[(level - 1) * 3 + 0];
		bitmap<T>& lout = this->buffers[(level - 1) * 3 + 1];

		bitmap<T> hhsrc = preprocess_subband(subbands.size() - level * 3 + 2, subband_rows_offset);
		bitmap<T> hlsrc = preprocess_subband(subbands.size() - level * 3 + 0, subband_rows_offset);
		bitmap<T> lhsrc = preprocess_subband(subbands.size() - level * 3 + 1, subband_rows_offset);

		this->transform(lhsrc, ll_collection[level], lout, disturbed_top_rows_num);
		this->transform(hhsrc, hlsrc, hout, disturbed_top_rows_num);
		this->transform(hout, lout, ll_collection[level - 1]);

		subband_overlap <<= 1;
		buffers_overlap <<= 1;
		buffers_overlap = std::min(constants::dwt::lowpass_dependency_radius, buffers_overlap);
	}

	// bottom cut is just a shrink by number of target image rows on the transformed output, constant time.
	// top cut is overlay offset, counting proper intermediate drop offsets
	// bottom data should be present in the preceding fragment undisturbed. The corresponding top overlay 
	// data of the subseqent fragment should be dropped.
	// 
	// that is, on every pass we drop invalidated rows, to keep only valid data as the next level input.
	// on the last pass we drop all overlay rows, whether they are valid or not.
	// 
	// overlay minimum value is 6, along height/y axis, in LL3 dimensions: top overlay 4 and bottom overlay 3
	//

	// TODO: but technically this->transform transposes the output
	// in order to pass the output to the next level bdwt. It is 
	// possible to omit transpose there and then skip calling 
	// transpose below, 2 trasnpose ops in a row are redundant.

	// backward dwt already produces transposed output, in order to 
	// restore original image transpose is needed. Therefore inverted
	// logic here.
	if (!this->transpose_output) {
		return dst.transpose<oT>();
	}
	return static_cast<bitmap<oT>>(dst);
}

template <typename T, size_t alignment>
dwtscale<T>& BackwardWaveletTransformer<T, alignment>::get_scale() {
	return this->scale;
}

template <typename T, size_t alignment>
bool BackwardWaveletTransformer<T, alignment>::get_skip_dc_scaling() const {
	return this->skip_dc_scaling;
}

template <typename T, size_t alignment>
void BackwardWaveletTransformer<T, alignment>::set_skip_dc_scaling(bool value) {
	this->skip_dc_scaling = value;
}

template <typename T, size_t alignment>
bool BackwardWaveletTransformer<T, alignment>::get_transpose_output() const {
	return this->transpose_output;
}

template <typename T, size_t alignment>
void BackwardWaveletTransformer<T, alignment>::set_transpose_output(bool value) {
	this->transpose_output = value;
}

template <typename T, size_t alignment>
void BackwardWaveletTransformer<T, alignment>::transform(const bitmap<T>& hsrc, const bitmap<T>& lsrc, bitmap<T>& dst, size_t dst_drop_offset) {
	bool valid = true;
	valid &= hsrc.get_meta().width == lsrc.get_meta().width;
	valid &= hsrc.get_meta().height == lsrc.get_meta().height;
	if (!valid) {
		// TODO: handle error
	}

	size_t src_height = hsrc.get_meta().height;
	size_t src_width = hsrc.get_meta().width;
	size_t buffer_width = src_width * 2;	// or get rid of variable and use buffer.get_meta() for stack utilization sake?
	std::array<std::array<T, alignment>, alignment> transpose_buffer = {};
	bitmap<T> dstbuffer(buffer_width, alignment, 64);
	
	for (ptrdiff_t i = 0; i < src_height; i += alignment) { // used for input indexing, transposed for dst
		ptrdiff_t max_src_row_index = std::min<ptrdiff_t>(src_height - i, alignment);
		for (ptrdiff_t j = 0; j < max_src_row_index; ++j) {
			bitmap_row<T> hrow(hsrc[i + j]);
			bitmap_row<T> lrow(lsrc[i + j]);
			this->core.exthbwd(hrow.ptr());
			this->core.rexthbwd(hrow.ptr() + hrow.width());
			this->core.extlbwd(lrow.ptr());
			this->core.rextlbwd(lrow.ptr() + lrow.width());
			this->core.bwd(hrow.ptr(), lrow.ptr(), dstbuffer[j].ptr(), buffer_width / 2);
		}

		// debug block, inverse op
		if constexpr (dbg::dwt::decoder::if_enabled(dbg::dwt::mask_reverse_data)) {
			bitmap<T> lbuffer(buffer_width / 2, alignment, 32);
			bitmap<T> hbuffer(buffer_width / 2, alignment, 32);
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				bitmap_row<T> source_row(dstbuffer[j]);
				this->core.extfwd(source_row.ptr());
				this->core.rextfwd(source_row.ptr() + source_row.width());
				this->core.fwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr(), buffer_width / 2);
				this->core.corrhfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
				this->core.corrlfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
			}
			ptrdiff_t row_bound = lbuffer.get_meta().width / 2;
			if (row_bound < lsrc.get_meta().width) {
				row_bound -= 1;
			}
			for (ptrdiff_t ii = 0; ii < alignment; ++ii) {
				for (ptrdiff_t jj = 0; jj < row_bound; ++jj) {
					if ((hsrc[i + ii][jj] != hbuffer[ii][jj]) || 
						(lsrc[i + ii][jj] != lbuffer[ii][jj])) {
						throw "NEQ!";
					}
				}
			}
		}

		for (ptrdiff_t j = 0; j < dst.get_meta().height; j += alignment) {
			ptrdiff_t max_dst_row_index = std::min<ptrdiff_t>(dst.get_meta().height - j, alignment);
			for (ptrdiff_t ii = 0; ii < alignment; ++ii) {
				for (ptrdiff_t jj = 0; jj < alignment; ++jj) {
					transpose_buffer[jj][ii] = dstbuffer[ii][j + jj + dst_drop_offset];
				}
			}
			for (ptrdiff_t k = 0; k < max_dst_row_index; ++k) {
				bitmap_row<T> dstrow = dst[j + k];
				for (ptrdiff_t l = 0; l < alignment; ++l) {
					dstrow[i + l] = transpose_buffer[k][l];
				}
			}
		}
	}
}

template <typename T, size_t alignment>
void BackwardWaveletTransformer<T, alignment>::set_fragment_size(size_t width, size_t height) {
	bool valid = true;
	valid &= width >= constants::img::min_width;
	valid &= width <= constants::img::max_width;
	valid &= (width & constants::dwt::horizontal_padding_mask) == 0;
	valid &= (height & constants::dwt::vertical_padding_mask) == 0;
	if (!valid) {
		// TODO: validation error
	}

	constexpr size_t max_top_l3_overlap = constants::dwt::lowpass_dependency_radius >> 1;
	size_t LL3_width = width >> 3;
	size_t LL3_base_height = height >> 3;
	this->resize_buffers(LL3_width, LL3_base_height + max_top_l3_overlap, max_top_l3_overlap);
}

template <typename T, size_t alignment>
void BackwardWaveletTransformer<T, alignment>::resize_buffers(size_t width, size_t height, size_t top_overlap) {
	height -= top_overlap;
	// H1, L1, LL1, H2, L2, LL2, H3, L3
	for (ptrdiff_t level = constants::dwt::level_num; level > 0; --level) {
		if (level != 3) {
			this->buffers[((level - 1) * 3) + 2].resize(height + top_overlap, width);
		}

		height <<= 1;
		// top_overlap = zeropred(constants::dwt::lowpass_dependency_radius, (level != 1) & (top_overlap > 0));
		top_overlap <<= 1;
		top_overlap = std::min(constants::dwt::lowpass_dependency_radius, top_overlap);
		top_overlap = zeropred(top_overlap, level != 1);

		this->buffers[((level - 1) * 3) + 0].resize(width, height + top_overlap);
		this->buffers[((level - 1) * 3) + 1].resize(width, height + top_overlap);

		width <<= 1;
	}
}


//
// explicit instantiation section

template class ForwardWaveletTransformer<int8_t>;
template subbands_t<int8_t> ForwardWaveletTransformer<int8_t>::apply<int8_t>(const bitmap<int8_t>& source, const img_pos& frame);
template subbands_t<int8_t> ForwardWaveletTransformer<int8_t>::apply<uint8_t>(const bitmap<uint8_t>& source, const img_pos& frame);
template subbands_t<int8_t> ForwardWaveletTransformer<int8_t>::apply<int8_t>(bitmap<int8_t>& source);
template subbands_t<int8_t> ForwardWaveletTransformer<int8_t>::apply<uint8_t>(bitmap<uint8_t>& source);
template void ForwardWaveletTransformer<int8_t>::preprocess_image<int8_t>(bitmap<int8_t>& src);
template void ForwardWaveletTransformer<int8_t>::preprocess_image<uint8_t>(bitmap<uint8_t>& src);

template class ForwardWaveletTransformer<int16_t>;
template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<int16_t>(const bitmap<int16_t>& source, const img_pos& frame);
template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<int8_t>(const bitmap<int8_t>& source, const img_pos& frame);
template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<uint16_t>(const bitmap<uint16_t>& source, const img_pos& frame);
template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<uint8_t>(const bitmap<uint8_t>& source, const img_pos& frame);
template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<int16_t>(bitmap<int16_t>& source);
template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<int8_t>(bitmap<int8_t>& source);
template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<uint16_t>(bitmap<uint16_t>& source);
template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<uint8_t>(bitmap<uint8_t>& source);
template void ForwardWaveletTransformer<int16_t>::preprocess_image<int16_t>(bitmap<int16_t>& src);
template void ForwardWaveletTransformer<int16_t>::preprocess_image<int8_t>(bitmap<int8_t>& src);
template void ForwardWaveletTransformer<int16_t>::preprocess_image<uint16_t>(bitmap<uint16_t>& src);
template void ForwardWaveletTransformer<int16_t>::preprocess_image<uint8_t>(bitmap<uint8_t>& src);

template class ForwardWaveletTransformer<int32_t>;
template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<int32_t>(const bitmap<int32_t>& source, const img_pos& frame);
template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<int16_t>(const bitmap<int16_t>& source, const img_pos& frame);
template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<uint32_t>(const bitmap<uint32_t>& source, const img_pos& frame);
template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<uint16_t>(const bitmap<uint16_t>& source, const img_pos& frame);
template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<int32_t>(bitmap<int32_t>& source);
template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<int16_t>(bitmap<int16_t>& source);
template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<uint32_t>(bitmap<uint32_t>& source);
template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<uint16_t>(bitmap<uint16_t>& source);
template void ForwardWaveletTransformer<int32_t>::preprocess_image<int32_t>(bitmap<int32_t>& src);
template void ForwardWaveletTransformer<int32_t>::preprocess_image<int16_t>(bitmap<int16_t>& src);
template void ForwardWaveletTransformer<int32_t>::preprocess_image<uint32_t>(bitmap<uint32_t>& src);
template void ForwardWaveletTransformer<int32_t>::preprocess_image<uint16_t>(bitmap<uint16_t>& src);

template class ForwardWaveletTransformer<int64_t>;
template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<int64_t>(const bitmap<int64_t>& source, const img_pos& frame);
template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<int32_t>(const bitmap<int32_t>& source, const img_pos& frame);
template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<uint64_t>(const bitmap<uint64_t>& source, const img_pos& frame);
template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<uint32_t>(const bitmap<uint32_t>& source, const img_pos& frame);
template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<int64_t>(bitmap<int64_t>& source);
template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<int32_t>(bitmap<int32_t>& source);
template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<uint64_t>(bitmap<uint64_t>& source);
template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<uint32_t>(bitmap<uint32_t>& source);
template void ForwardWaveletTransformer<int64_t>::preprocess_image<int64_t>(bitmap<int64_t>& src);
template void ForwardWaveletTransformer<int64_t>::preprocess_image<int32_t>(bitmap<int32_t>& src);
template void ForwardWaveletTransformer<int64_t>::preprocess_image<uint64_t>(bitmap<uint64_t>& src);
template void ForwardWaveletTransformer<int64_t>::preprocess_image<uint32_t>(bitmap<uint32_t>& src);

template class ForwardWaveletTransformer<float>;
template subbands_t<float> ForwardWaveletTransformer<float>::apply<int32_t>(const bitmap<int32_t>& source, const img_pos& frame);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<int16_t>(const bitmap<int16_t>& source, const img_pos& frame);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<int8_t>(const bitmap<int8_t>& source, const img_pos& frame);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint32_t>(const bitmap<uint32_t>& source, const img_pos& frame);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint16_t>(const bitmap<uint16_t>& source, const img_pos& frame);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint8_t>(const bitmap<uint8_t>& source, const img_pos& frame);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<int32_t>(bitmap<int32_t>& source);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<int16_t>(bitmap<int16_t>& source);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<int8_t>(bitmap<int8_t>& source);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint32_t>(bitmap<uint32_t>& source);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint16_t>(bitmap<uint16_t>& source);
template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint8_t>(bitmap<uint8_t>& source);
template void ForwardWaveletTransformer<float>::preprocess_image<int32_t>(bitmap<int32_t>& src);
template void ForwardWaveletTransformer<float>::preprocess_image<int16_t>(bitmap<int16_t>& src);
template void ForwardWaveletTransformer<float>::preprocess_image<int8_t>(bitmap<int8_t>& src);
template void ForwardWaveletTransformer<float>::preprocess_image<uint32_t>(bitmap<uint32_t>& src);
template void ForwardWaveletTransformer<float>::preprocess_image<uint16_t>(bitmap<uint16_t>& src);
template void ForwardWaveletTransformer<float>::preprocess_image<uint8_t>(bitmap<uint8_t>& src);

template class ForwardWaveletTransformer<double>;
template subbands_t<double> ForwardWaveletTransformer<double>::apply<int64_t>(const bitmap<int64_t>& source, const img_pos& frame);
template subbands_t<double> ForwardWaveletTransformer<double>::apply<int32_t>(const bitmap<int32_t>& source, const img_pos& frame);
template subbands_t<double> ForwardWaveletTransformer<double>::apply<uint64_t>(const bitmap<uint64_t>& source, const img_pos& frame);
template subbands_t<double> ForwardWaveletTransformer<double>::apply<uint32_t>(const bitmap<uint32_t>& source, const img_pos& frame);
template subbands_t<double> ForwardWaveletTransformer<double>::apply<int64_t>(bitmap<int64_t>& source);
template subbands_t<double> ForwardWaveletTransformer<double>::apply<int32_t>(bitmap<int32_t>& source);
template subbands_t<double> ForwardWaveletTransformer<double>::apply<uint64_t>(bitmap<uint64_t>& source);
template subbands_t<double> ForwardWaveletTransformer<double>::apply<uint32_t>(bitmap<uint32_t>& source);
template void ForwardWaveletTransformer<double>::preprocess_image<int64_t>(bitmap<int64_t>& src);
template void ForwardWaveletTransformer<double>::preprocess_image<int32_t>(bitmap<int32_t>& src);
template void ForwardWaveletTransformer<double>::preprocess_image<uint64_t>(bitmap<uint64_t>& src);
template void ForwardWaveletTransformer<double>::preprocess_image<uint32_t>(bitmap<uint32_t>& src);
// template class ForwardWaveletTransformer<long double>;


template class BackwardWaveletTransformer<int8_t>;
template bitmap<int8_t> BackwardWaveletTransformer<int8_t>::apply<int8_t>(subbands_t<int8_t>& subbands, size_t top_overlap);
template bitmap<uint8_t> BackwardWaveletTransformer<int8_t>::apply<uint8_t>(subbands_t<int8_t>& subbands, size_t top_overlap);

template class BackwardWaveletTransformer<int16_t>;
template bitmap<int16_t> BackwardWaveletTransformer<int16_t>::apply<int16_t>(subbands_t<int16_t>& subbands, size_t top_overlap);
template bitmap<int8_t> BackwardWaveletTransformer<int16_t>::apply<int8_t>(subbands_t<int16_t>& subbands, size_t top_overlap);
template bitmap<uint16_t> BackwardWaveletTransformer<int16_t>::apply<uint16_t>(subbands_t<int16_t>& subbands, size_t top_overlap);
template bitmap<uint8_t> BackwardWaveletTransformer<int16_t>::apply<uint8_t>(subbands_t<int16_t>& subbands, size_t top_overlap);

template class BackwardWaveletTransformer<int32_t>;
template bitmap<int32_t> BackwardWaveletTransformer<int32_t>::apply<int32_t>(subbands_t<int32_t>& subbands, size_t top_overlap);
template bitmap<int16_t> BackwardWaveletTransformer<int32_t>::apply<int16_t>(subbands_t<int32_t>& subbands, size_t top_overlap);
template bitmap<uint32_t> BackwardWaveletTransformer<int32_t>::apply<uint32_t>(subbands_t<int32_t>& subbands, size_t top_overlap);
template bitmap<uint16_t> BackwardWaveletTransformer<int32_t>::apply<uint16_t>(subbands_t<int32_t>& subbands, size_t top_overlap);

template class BackwardWaveletTransformer<int64_t>;
template bitmap<int64_t> BackwardWaveletTransformer<int64_t>::apply<int64_t>(subbands_t<int64_t>& subbands, size_t top_overlap);
template bitmap<int32_t> BackwardWaveletTransformer<int64_t>::apply<int32_t>(subbands_t<int64_t>& subbands, size_t top_overlap);
template bitmap<uint64_t> BackwardWaveletTransformer<int64_t>::apply<uint64_t>(subbands_t<int64_t>& subbands, size_t top_overlap);
template bitmap<uint32_t> BackwardWaveletTransformer<int64_t>::apply<uint32_t>(subbands_t<int64_t>& subbands, size_t top_overlap);


template class BackwardWaveletTransformer<float>;
template bitmap<int32_t> BackwardWaveletTransformer<float>::apply<int32_t>(subbands_t<float>& subbands, size_t top_overlap);
template bitmap<int16_t> BackwardWaveletTransformer<float>::apply<int16_t>(subbands_t<float>& subbands, size_t top_overlap);
template bitmap<int8_t> BackwardWaveletTransformer<float>::apply<int8_t>(subbands_t<float>& subbands, size_t top_overlap);
template bitmap<uint32_t> BackwardWaveletTransformer<float>::apply<uint32_t>(subbands_t<float>& subbands, size_t top_overlap);
template bitmap<uint16_t> BackwardWaveletTransformer<float>::apply<uint16_t>(subbands_t<float>& subbands, size_t top_overlap);
template bitmap<uint8_t> BackwardWaveletTransformer<float>::apply<uint8_t>(subbands_t<float>& subbands, size_t top_overlap);

template class BackwardWaveletTransformer<double>;
template bitmap<int64_t> BackwardWaveletTransformer<double>::apply<int64_t>(subbands_t<double>& subbands, size_t top_overlap);
template bitmap<int32_t> BackwardWaveletTransformer<double>::apply<int32_t>(subbands_t<double>& subbands, size_t top_overlap);
template bitmap<uint64_t> BackwardWaveletTransformer<double>::apply<uint64_t>(subbands_t<double>& subbands, size_t top_overlap);
template bitmap<uint32_t> BackwardWaveletTransformer<double>::apply<uint32_t>(subbands_t<double>& subbands, size_t top_overlap);
// template class BackwardWaveletTransformer<long double>;


//
// non-member functions implementation

namespace {

	template <typename T, size_t alignment>
	void copy_lowpass_data(const bitmap<T, alignment>& src, bitmap<T, alignment>& dst, std::array<row_range, 2> ranges) {
		img_meta target_dimensions = dst.get_meta();
		for (ptrdiff_t j = 0; j < target_dimensions.height; ++j) {
			bitmap_row<T> src_row = src[j];
			bitmap_row<T> dst_row = dst[j];

			ptrdiff_t read_offset = ranges[0].offset;
			ptrdiff_t k = 0;
			for (; k < ranges[0].size; k += alignment) {
				for (ptrdiff_t l = 0; l < alignment; ++l) {
					// writes aligned, reads misaligned
					dst_row[k + l] = src_row[k + l + read_offset];
				}
			}

			// align dst tail prior to continuing writing
			read_offset += ranges[1].offset;
			for (ptrdiff_t l = ranges[0].size; l < k; ++l) {
				dst_row[l] = src_row[l + read_offset];
			}

			for (; k < target_dimensions.width; k += alignment) {
				for (ptrdiff_t l = 0; l < alignment; ++l) {
					// writes aligned, reads misaligned
					dst_row[k + l] = src_row[k + l + read_offset];
				}
			}
		}
	}

}
