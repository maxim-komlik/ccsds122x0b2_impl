#pragma once

#include <array>

#include "core_types.hpp"
#include "bitmap.tpp"
#include "dwtcore.tpp"
#include "dwtscale.tpp"
#include "img_meta.hpp"
#include "constant.hpp"

template <typename T, size_t alignment = 16>
class ForwardWaveletTransformer: private constants::dwt {
	// buffers are allocated in the order below:
	// H1, L1, LL1, H2, L2, LL2, H3, L3, LL3, HL3, LH3, HH3, HL2, LH2, HH2, HL1, LH1, HH1
	// 
	// the first 9 buffers are reusable for the given frame size; the rest contain output
	std::array<bitmap<T>, 18> buffers; 

	dwtcore<T> core;
	dwtscale<T> scale;

	bool LL3_subband_only = false;
	bool parallel_compute = false;

public:
	ForwardWaveletTransformer();

	template <typename iT>
	void preprocess_image(bitmap<iT>& src);

	template <typename iT>
	subbands_t<T> apply(const bitmap<iT>& source, const img_pos& frame);

	template <typename iT>	// image type and DWT underlying type may differ
	subbands_t<T> apply(bitmap<iT>& source);

	// TODO: redesign scaling configuration [const correctness]
	dwtscale<T>& get_scale();

	void set_target_frame_size(size_t width, size_t height);

private:
	union frame_overlap_description;

	template <typename iT>
	void transform(const_bitmap_slice<iT> source, bitmap_slice<T> hdst, bitmap_slice<T> ldst,
		ptrdiff_t hdst_drop_offset = 0, ptrdiff_t ldst_drop_offset = 0,
		bool skip_extension = false);
	std::array<img_pos, 2> decompose_frame(img_meta src_dims, img_pos target_frame);
	void resize_buffers(size_t base_width, size_t base_height, std::array<frame_overlap_description, 2>& overlap);
};


template <typename T, size_t alignment = 16>
class BackwardWaveletTransformer: private constants::dwt {	
	// buffers are allocated in the order below:
	// H1, L1, LL1, H2, L2, LL2, H3, L3
	std::array<bitmap<T>, 8> buffers;
	dwtcore<T> core;
	dwtscale<T> scale;

	bool skip_dc_scaling = true;
	bool transpose_output = false;

public:
	BackwardWaveletTransformer();

	template <typename oT = T>
	bitmap<oT> apply(subbands_t<T>& subbands, size_t dc_top_overlap = 0);	// TODO: const ref parameter?

	// TODO: redesign scaling configuration [const correctness]
	dwtscale<T>& get_scale();

	bool get_skip_dc_scaling() const;
	void set_skip_dc_scaling(bool value);

	bool get_transpose_output() const;
	void set_transpose_output(bool value);

	void set_fragment_size(size_t width, size_t height);

private:
	void transform(const bitmap<T>& hsrc, const bitmap<T>& lsrc, bitmap<T>& dst, size_t dst_drop_offset = 0);
	void resize_buffers(size_t LL3_width, size_t LL3_height, size_t l3_top_overlap);
};

// TODO: review and check if needed/edit
// General implementation design comments

//
// single transformer instance may be used across several tasks that perform dwt transform 
// on different parts of single input image (different subbands or different regions of a 
// single subband, if image is processed by regions, i.e. moving frame), or even different 
// images of different sessions.
// use of single instance allows to reuse memory buffers for intermediate dwt results, if 
// an instance is used with the same processing frame size parameters.
// 
// the size of reusable buffers is (2.5 x image size)
// the rest of buffers are actually outpurs, and they're moved out on every transform call, 
//		meaning that actually they are not properties and has to be allocated on every call.
//		they occupy (1 x image size) in total
// 
// we'd like to separate memory management from heavy computations, if possible.
// an interface to configure frame parameters before passing input data is needed.
// at the same time, we somehow should validate buffer size on every input pass.
// 
// 
// technique that uses frame position as transformer's property and extension buffers 
// to carry HH subbands across different calls on subsequent regions of the input image 
// implies that the same transformer is used for consequtive image transormation operations.
// That strengthen data dependency, requires additional buffers, necessiates mechanism to 
// maintain and properly configure the state of the transformer between consequtive calls 
// and provides no benefit performance-wise, because segment assembly is still blocked by 
// non-transformed image data (except the case when width of a transform frame is the same 
// as input image width, but that scenario makes half of the extension buffers redundant); 
// the only benefit is possibly reduced memory usage (with nearly the same computational 
// cost, in return of excessive copy operations, that is, memory performance).
// Therefore stateful transform frame tracking is not supported by the implementation.
//		img_pos m_frame_pos;
//		std::array<bitmap<T>, 4> v_extension_buffers;
//			// buffers for upper and left "prefixes" of HH1 and HH2 intermediate results 
//			// of the frame being processed, that stores elements from the previous call 
//			// that are needed for current frame dwt transform.
//			// but actually can be implemented as extended intermediate buffers themselves, 
//			// meaning that buffers for HH1 and HH2 can have additional rows and columns, 
//			// and the actual current data in the buffer located somewhere at x- and y- 
//			// axes offsets equal to dwt overlap area.
//

// On transforming the image by fragments:
// two possible approaches: 
//		+ configured frame, used to slice a source image; single source image region is 
//			accessed by all the transformers involved in operation
//		+ source image bitmap is split into several bitmaps, every bitmap has independent 
//			storage, all these bitmaps have overlapped regions (thus resulting in excessive 
//			memory allocations)
// dwt requires input signal to have even size so that lowpass data and highpass data results 
// have the same dimensions, for 3-level dwt this implies that input image's dimensions has 
// to be multiple of 8. Each dwt result coefficient depends on input data at indicies [-4, +4] 
// relatively to the target item; 
// 
// bitmap fragments:
//		- continious fragment that lasts across right image border is not possible, meaning 
//			that each fragment row ends with a segment with decreased width, i.e. 
//			intermediate fragments have different dimensions
//		= fragment dwt processing is unchanged compared to single fragment case, but logic 
//			to combine different fragments' dwt output subbands together becomes complicated.
//		= requires extended buffers to contain overlapped regions of intermediate dwt results
//		- dimensions of the fragment buffers in the first fragment row are different from 
//			other fragments, resulting in 9 different possible dimension settings (because the 
//			first row doesn't have overlapping upper region, as well as the last row doesn't 
//			have bottom overlapping, same is true for segments along right and left border)
//		- redundant buffer dwt coefficients value extension steps for every segment (really 
//			needed for fragments along one of the borders only)
//		+ each thread has independent input buffer that can be written to
//		- overlapped regions of 20 image rows on every fragment border are copied at least to 
//			2 different buffers, which constitutes significant overhead
// 
//	each bitmap has overlap regions on every side (except those that lie along image border).
//	all the unnecessary coefficients are dropped when fragments are to be combined; those 
//	appear due to redundant extensions and duplicate coefficients data.
// 
// reference to the image data with frame settings:
//		- requires explicit image preprocessing call that extends all rows of the image 
//			before it can be transformed; as a result, first dwt level has to be handled 
//			differently
//		- still requires extended intermediate buffers to accomodate intermediate overlap 
//			coefficients, that have to be dropped at the transform finish
//		+ supports fragment of arbitrary dimensions (and geometry), that may lie across 
//			right image boundary (but a bit special handling required)
//		=- intensive concurrent memory reads from different threads on everlap regions
//		- requires special handling of fragment borders: either extension, either overlap
// 
//	based on fragment properties, for every fragment border a dedicated setting is 
//	computed to describe whether extension or overlap handling should be applied to 
//	all the corresponding borders of all intermediate buffers, considering transposed 
//	layout of some internal buffers (although overlap handling can be implemented as 
//	result buffers slice so that all the unnecessary items are dropped, to maintain 
//	proper aligment of data in the intermediate buffers to allow effective transposition).
//	Intermediate buffers should be configured to contain several aligment buffers for 
//	a fragment that lays across image boundary, so that data at row's end and start can be 
//	extended while keeping start of the row aligned properly, so that extension data on 
//	every end does not interfere with significant counterpart. The alignment gap has to 
//	be removed then when the output buffers are built
//
