#pragma once

namespace forward {
	template <typename T = double>
	constexpr T h[] = {
		0.852698679009,
		0.377402855613,
		-0.110624404418,
		-0.023849465020,
		0.037828455507
	};

	template <typename T = double>
	constexpr T g[] = {
		-0.788485616406,
		0.418092273222,
		0.040689417609,
		-0.064538882629
	};
}

namespace inverse {
	template <typename T = double>
	constexpr T q[] = {
		0.788485616406,
		0.418092273222,
		-0.040689417609,
		-0.064538882629
	};

	template <typename T = double>
	constexpr T p[] = {
		-0.852698679009,
		0.377402855613,
		0.110624404418,
		-0.023849465020,
		-0.037828455507
	};
}

#include <array>
#include <vector>
#include <utility>

#include "core_types.h"
#include "bitmap.tpp"
#include "dwtcore.tpp"
#include "dwtscale.tpp"

struct img_pos {
	size_t x;
	size_t x_step;
	size_t y;
	size_t y_step;
	size_t z;
	size_t z_step;
	size_t x_stride;
	size_t width;
	size_t y_stride;
	size_t height;
	size_t z_stride;
	size_t depth;
};

template <typename T, size_t alignment = 16>
class ForwardWaveletTransformer {
	std::array<typename bitmap<T>, 18> buffers;
	std::array<img_meta, 3> current_frames;
	dwtcore<T> core;

	std::array<bitmap<T>, 6> v_extension_buffers;

	img_meta m_src_meta;
	img_pos m_frame_pos;

	dwtscale<T> scale;

	static constexpr size_t c_h_alignment = 8;
	static constexpr size_t c_v_alignment = 8;
	static constexpr size_t c_level_count = 3;

	static constexpr size_t c_families_count = 3;
	static constexpr size_t c_LL3_offset = 8;
public:
	// TODO: MAJOR: refactor constructors. We're going to explicitly create 
	// instances for all applicable dypes/settings in a library to later use
	// them in a client binary. This means we cannot pass data-dependent values 
	// as constructor options. Split constructors into constructor and 
	// data-dependent initializer that performs necessary data-specific setup.
	// 
	// This is applicable to most classes in the project.
	//
	ForwardWaveletTransformer(img_pos frame_properties);

	// TODO: takes source image as input only
	// Image entry point moves transform frame accross the whole image
	// Manages additional buffers that depends on input image properties.
	void apply(bitmap<T>& source);
	subbands_t<T> get_subbands();

	// TODO: redesign scaling configuration [const correctness]
	dwtscale<T>& get_scale();

private:
	void transform(const bitmap<T>& source, bitmap<T>& hdst, bitmap<T>& ldst);
	void ext(bitmap<T>& source, bitmap<T>& hdst, bitmap<T>& ldst, size_t i);
};

template <typename T, size_t alignment = 16>
class BackwardWaveletTransformer {
	std::array<typename bitmap<T>, 18> buffers;
	std::array<img_meta, 3> current_frames;
	dwtcore<T> core;

	std::array<bitmap<T>, 6> v_extension_buffers;

	img_meta m_src_meta;
	img_pos m_frame_pos;

	dwtscale<T> scale;

	bool skip_dc_scaling = true;
	bool transpose_output = false;

	static constexpr size_t c_h_alignment = 8;
	static constexpr size_t c_v_alignment = 8;
	static constexpr size_t c_level_count = 3;

	static constexpr size_t c_families_count = 3;
	static constexpr size_t c_LL3_offset = 8;

public:
	BackwardWaveletTransformer(img_pos frame_properties, bool scale_dc_props = true);

	// TODO: Image entry point moves transform frame accross the whole image
	// Manages additional buffers that depends on input image properties.
	bitmap<T> apply();
	void set_subbands(const subbands_t<T>& subbands);
	dwtscale<T>& get_scale();
	bool get_skip_dc_scaling() const;
	void set_skip_dc_scaling(bool value);
	bool get_transpose_output() const;
	void set_transpose_output(bool value);

private:
	void transform(const bitmap<T>& hsrc, const bitmap<T>& lsrc, bitmap<T>& dst);
};

// ForwardWaveletTransformer class template implementation

template <typename T, size_t alignment>
ForwardWaveletTransformer<T, alignment>::ForwardWaveletTransformer(img_pos frame_properties) {
	this->m_frame_pos.width = (frame_properties.width + (this->c_h_alignment - 1))
		& (~(this->c_h_alignment - 1));
	this->m_frame_pos.height = ((frame_properties.height + (this->c_v_alignment - 1))
		& (~(this->c_v_alignment - 1)));	// / 2;

	// Buffer capacity tuning should be based on frame properties, not image properties
	// Frame properties are configured when constructor is called.
	// Constructor takes frame properties as input parameter.
	constexpr size_t buffer_iter_step = 3;

	size_t width = this->m_frame_pos.height;
	size_t height = this->m_frame_pos.width / 2;
	for (size_t i = 0; i < this->c_level_count; ++i) {
		for (size_t j = 0; j < 2; ++j) {
			this->buffers[i * buffer_iter_step + j].resize(width, height);
		}
		width ^= height;
		height ^= width;
		width ^= height;
		height /= 2;
		this->buffers[i * buffer_iter_step + 2].resize(width, height);
		for (size_t j = 0; j < 3; ++j) {
			this->buffers[this->buffers.size() - (i + 1) * buffer_iter_step + j].resize(width, height);
		}
		width ^= height;
		height ^= width;
		width ^= height;
		height /= 2;
	}
}

template <typename T, size_t alignment>
void ForwardWaveletTransformer<T, alignment>::apply(bitmap<T>& source) {
	this->m_src_meta = source.get_meta();
	this->m_src_meta.width = (this->m_src_meta.width + (this->c_h_alignment - 1))
		& (~(this->c_h_alignment - 1));
	if (source.get_meta().width < this->m_src_meta.width) {
		for (size_t i = 0; i < this->m_src_meta.height; ++i) {
			bitmap_row<T> source_row = source[i];
			size_t start_index = source.get_meta().width;
			for (size_t j = start_index; j < this->m_src_meta.width + 1; ++j) {
				source_row[j] = source_row[start_index - 1];
			}
		}
	}
	// size_t level = 0;
	bitmap<T>* src = &source;
	for (ptrdiff_t level = 0; level < this->c_level_count; ++level) {
		bitmap<T>& hout = this->buffers[level * 3 + 0];
		bitmap<T>& lout = this->buffers[level * 3 + 1];
		bitmap<T>& llout = this->buffers[level * 3 + 2];
		bitmap<T>& hhout = this->buffers[this->buffers.size() - (level + 1) * 3 + 2];
		bitmap<T>& hlout = this->buffers[this->buffers.size() - (level + 1) * 3 + 0];
		bitmap<T>& lhout = this->buffers[this->buffers.size() - (level + 1) * 3 + 1];
		// Main chain
		this->transform(*src, hout, lout);
		this->transform(lout, lhout, llout);
		// Aux chain
		this->transform(hout, hhout, hlout);

		src = &llout;
	}
}

template <typename T, size_t alignment>
subbands_t<T> ForwardWaveletTransformer<T, alignment>::get_subbands() {
	// TODO: Check if possible to omit default-initialization and 
	// move-initialize members of output variable directly
	// [because aggregate initialization copy-initializes elements]
	subbands_t<T> result;
	for (ptrdiff_t i = 0; i < result.size(); ++i) {
		result[i] = std::move(this->buffers[c_LL3_offset + i]);
		this->scale.scale(result[i], i);
	}
	return result;
}

template <typename T, size_t alignment>
dwtscale<T>& ForwardWaveletTransformer<T, alignment>::get_scale() {
	return this->scale;
}

template <typename T, size_t alignment>
void ForwardWaveletTransformer<T, alignment>::transform(const bitmap<T>& source, bitmap<T>& hdst, bitmap<T>& ldst) {
	// Change this->m_src_meta to source.get_meta();
	img_meta source_meta = source.get_meta();
	size_t buffer_width = source_meta.width / 2;
	bitmap<T> lbuffer(buffer_width, alignment, 32);
	bitmap<T> hbuffer(buffer_width, alignment, 32);
	ptrdiff_t i = 0;
	for (; i < source_meta.height; i += alignment) {
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			bitmap_row<T> source_row(source[i + j]);
			this->core.extfwd(source_row.ptr());
			this->core.rextfwd(source_row.ptr() + source_row.width());
			this->core.fwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr(), buffer_width);
			this->core.corrhfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
			this->core.corrlfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
		}

		// TODO: implement constexpr if to enable the block below in 
		// debug builds only
		// 
		// debug block, inverse op
		{
			bitmap<T> dstbuffer(source_meta.width, alignment, 32);
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
				for (ptrdiff_t jj = 0; jj < source_meta.width; ++jj) {
					if (source[i + ii][jj] != dstbuffer[ii][jj]) {
						throw "NEQ!";
					}
				}
			}
		}
		for (ptrdiff_t k = 0; k < buffer_width; ++k) {
			bitmap_row<T> hrow = hdst[k];
			bitmap_row<T> lrow = ldst[k];
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				hrow[i + j] = hbuffer[j][k];
				lrow[i + j] = lbuffer[j][k];
			}
		}
	}
}

template <typename T, size_t alignment>
void ForwardWaveletTransformer<T, alignment>::ext(bitmap<T>& source, bitmap<T>& hdst, bitmap<T>& ldst, size_t i) {
	// Change this->m_src_meta to source.get_meta();
	img_meta source_meta = source.get_meta();
	size_t buffer_width = source_meta.width / 2;
	bitmap<T> lbuffer(buffer_width, alignment, 32);
	bitmap<T> hbuffer(buffer_width, alignment, 32);
	size_t j = 0;
	size_t base = i;
	for (; i < source_meta.height; ++i, ++j) {
		bitmap_row<T> source_row = source[i + j];
		this->core.extfwd(source_row.ptr());
		this->core.rextfwd(source_row.ptr() + source_meta.width);
		this->core.fwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr(), buffer_width);
		this->core.corrhfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
		this->core.corrlfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
	}
	size_t last = j - 1;
	// Alternative way for extension is possible (row extension after transpose)
	// But this way transpose vectorization is leveraged
	size_t ext_bounding = ((j + (this->c_v_alignment - 1)) & (~(this->c_v_alignment - 1)));
	for (; j < ext_bounding; ++j) {
		lbuffer[j].assign(lbuffer[last]);
		hbuffer[j].assign(hbuffer[last]);
	}
	for (; j < alignment; ++j) {
		lbuffer[j].assign(0);
		hbuffer[j].assign(0);
	}
	for (size_t k = 0; k < buffer_width; ++k) {
		bitmap_row<T> hrow = hdst[k];
		bitmap_row<T> lrow = ldst[k];
		for (j = 0; j < alignment; ++j) {
			hrow[base + j] = hbuffer[j][k];
			lrow[base + j] = lbuffer[j][k];
		}
	}
}


// BackwardWaveletTransformer class template implementation

template <typename T, size_t alignment>
BackwardWaveletTransformer<T, alignment>::BackwardWaveletTransformer(img_pos frame_properties, bool scale_dc_props) {
	// TODO: refactor this? Tried to fix issues caused by not scaled
	// down frame_properties (passed the same as for forward dwt) and 
	// rewrote all the related stuff =)
	// Actually we don't need to pass expected output image dimensions
	// as an input parameter in production code, only used in tests.
	// frame_properties is DC subband's meta by design of segment 
	// decoding, but may be expected output image meta if bool input 
	// parameter set to true.
	if (scale_dc_props) {
		frame_properties.width <<= 3;
		frame_properties.height <<= 3;
	}
	this->m_frame_pos.width = frame_properties.width;
	this->m_frame_pos.height = frame_properties.height;
	constexpr size_t buffer_iter_step = 3;

	size_t height = this->m_frame_pos.height;
	size_t width = this->m_frame_pos.width / 2;
	// for (size_t i = this->c_level_count; i > 0; --i) {
	for (size_t i = 0; i < this->c_level_count; ++i) {
		for (size_t j = 0; j < 2; ++j) {
			this->buffers[i * buffer_iter_step + j].resize(width, height);
		}
		width ^= height;
		height ^= width;
		width ^= height;
		width /= 2;

		this->buffers[i * buffer_iter_step + 2].resize(width, height); // TODO: overlaps DC subband
		width ^= height;
		height ^= width;
		width ^= height;
		width /= 2;
	}
}

template <typename T, size_t alignment>
bitmap<T> BackwardWaveletTransformer<T, alignment>::apply() {
	size_t width = this->buffers[0].get_meta().height;
	size_t height = this->buffers[0].get_meta().width * 2;
	bitmap<T> dst(width, height);

	bitmap<T>* dst_collection[this->c_level_count];
	for (size_t i = 1; i < this->c_level_count; ++i) {
		dst_collection[i] = &(this->buffers[(i - 1) * 3 + 2]);
	}
	dst_collection[0] = &dst;
	for (ptrdiff_t level = 2 /*this->c_level_count - 1*/; level >= 0; --level) {
		bitmap<T>& hout = this->buffers[level * 3 + 0];
		bitmap<T>& lout = this->buffers[level * 3 + 1];
		bitmap<T>& llout = this->buffers[level * 3 + 2];
		bitmap<T>& hhout = this->buffers[this->buffers.size() - (level + 1) * 3 + 2];
		bitmap<T>& hlout = this->buffers[this->buffers.size() - (level + 1) * 3 + 0];
		bitmap<T>& lhout = this->buffers[this->buffers.size() - (level + 1) * 3 + 1];

		// concurrent run
		this->transform(lhout, llout, lout);
		this->transform(hhout, hlout, hout);
		// blocking op
		this->transform(hout, lout, *(dst_collection[level]));
	}

	// TODO: but techically this->transform transposes the output
	// in order to pass the output to the next level bdwt. It is 
	// possible to omit transpose there and then skip calling 
	// transpose below, 2 trasnpose ops in a row are redundant.

	// backward dwt already produces transposed output, in order to 
	// restore original image transpose is needed. Therefore inverted
	// logic here.
	if (!this->transpose_output) {
		dst = dst.transpose();
	}
	// TODO: fix rvalue reference, so object is moved and not copied
	return dst;
}

template <typename T, size_t alignment>
void BackwardWaveletTransformer<T, alignment>::set_subbands(const subbands_t<T>& subbands) {
	// skip DC scaling as DC are decoded in a separate buffers and are
	// scaled properly already in BPE.
	ptrdiff_t i = 0;
	if (this->skip_dc_scaling) {
		this->buffers[c_LL3_offset + i] = subbands[i].transpose(); // here is implicit bitmap copy
		i = 1;
	}
	for (; i < subbands.size(); ++i) {
		this->buffers[c_LL3_offset + i] = subbands[i].transpose(); // here is implicit bitmap copy
		this->scale.unscale(this->buffers[c_LL3_offset + i], i);
	}
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
void BackwardWaveletTransformer<T, alignment>::transform(const bitmap<T>& hsrc, const bitmap<T>& lsrc, bitmap<T>& dst) {
	img_meta dst_meta = dst.get_meta();
	size_t buffer_width = dst_meta.height;
	bitmap<T> dstbuffer(buffer_width, alignment, 32);
	ptrdiff_t i = 0;
	for (; i < dst_meta.width; i += alignment) { // because transposed
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			bitmap_row<T> hrow(hsrc[i + j]);
			bitmap_row<T> lrow(lsrc[i + j]);
			this->core.exthbwd(hrow.ptr());
			this->core.rexthbwd(hrow.ptr() + hrow.width());
			this->core.extlbwd(lrow.ptr());
			this->core.rextlbwd(lrow.ptr() + lrow.width());
			this->core.bwd(hrow.ptr(), lrow.ptr(), dstbuffer[j].ptr(), buffer_width / 2);
		}

		// TODO: implement constexpr if to enable the block below in 
		// debug builds only
		// 
		// debug block, inverse op
		{
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

		for (ptrdiff_t k = 0; k < buffer_width; ++k) {
			bitmap_row<T> dstrow = dst[k];
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				dstrow[i + j] = dstbuffer[j][k];
			}
		}
	}
}
