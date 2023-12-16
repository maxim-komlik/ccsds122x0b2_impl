#pragma once

#include <algorithm>
#include <vector>
#include <valarray>
#include "bitmap.tpp"

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

template <size_t size_bytes>
struct _sufficient_integral;

template <>
struct _sufficient_integral<2> {
	typedef int_fast16_t type;
};

template <>
struct _sufficient_integral<4> {
	typedef int_fast32_t type;
};

template <>
struct _sufficient_integral<8> {
	typedef int_fast64_t type;
};

template <typename T>
using sufficient_integral = _sufficient_integral<sizeof(T)>;

#include "bitmap.tpp"
#include "dwtcore.tpp"
#include <array>
#include <vector>

template <typename T>
union block {
	T content[64];
};

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

template <typename T, size_t allignment = 16>
class ForwardWaveletTransformer {
	std::array<typename bitmap<T>, 18> buffers;
	std::array<img_meta, 3> current_frames;
	dwtcore<T> core;

	std::array<bitmap<T>, 6> v_extension_buffers;

	img_meta m_src_meta;
	img_pos m_frame_pos;

	static const size_t c_h_allignment = 8;
	static const size_t c_v_allignment = 8;
	static const size_t c_level_count = 3;

	static const size_t c_families_count = 3;
	static const size_t c_LL3_offset = 8;
public:
	ForwardWaveletTransformer(img_pos frame_properties);

	// TODO: takes source image as input only
	// Image entry point moves transform frame accross the whole image
	// Manages additional buffers that depends on input image properties.
	std::vector<block<T>> apply(bitmap<T>& source);

	auto _getBuffers() {
		return this->buffers;
	}

private:
	void transform(const bitmap<T>& source, bitmap<T>& hdst, bitmap<T>& ldst);
	std::vector<block<T>> pack();

	void ext(bitmap<T>& source, bitmap<T>& hdst, bitmap<T>& ldst, size_t i);
};


template <typename T, size_t allignment>
ForwardWaveletTransformer<T, allignment>::ForwardWaveletTransformer(img_pos frame_properties) {
	this->m_frame_pos.width = (frame_properties.width + (this->c_h_allignment - 1))
		& (~(this->c_h_allignment - 1));
	this->m_frame_pos.height = ((frame_properties.height + (this->c_v_allignment - 1))
		& (~(this->c_v_allignment - 1)));	// / 2;

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

template <typename T, size_t allignment>
std::vector<block<T>> ForwardWaveletTransformer<T, allignment>::apply(bitmap<T>& source) {
	this->m_src_meta = source.getImgMeta();
	this->m_src_meta.width = (this->m_src_meta.width + (this->c_h_allignment - 1))
		& (~(this->c_h_allignment - 1));
	if (source.getImgMeta().width < this->m_src_meta.width) {
		for (size_t i = 0; i < this->m_src_meta.height; ++i) {
			bitmap_row<T> source_row = source[i];
			size_t start_index = source.getImgMeta().width;
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
	return this->pack();
}

template <typename T, size_t allignment>
void ForwardWaveletTransformer<T, allignment>::transform(const bitmap<T>& source, bitmap<T>& hdst, bitmap<T>& ldst) {
	// Change this->m_src_meta to source.getImgMeta();
	img_meta source_meta = source.getImgMeta();
	size_t buffer_width = source_meta.width / 2;
	bitmap<T> lbuffer(buffer_width, allignment, 32);
	bitmap<T> hbuffer(buffer_width, allignment, 32);
	size_t i = 0;
	for (; i < source_meta.height; i += allignment) {
		for (size_t j = 0; j < allignment; ++j) {
			bitmap_row<T> source_row(source[i + j]);
			this->core.extfwd(source_row.ptr());
			this->core.rextfwd(source_row.ptr() + source_row.width());
			this->core.fwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr(), buffer_width);
			this->core.corrhfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
			this->core.corrlfwd(source_row.ptr(), hbuffer[j].ptr(), lbuffer[j].ptr());
		}
		// debug block
		{
			bitmap<T> dstbuffer(source_meta.width, allignment, 32);
			for (size_t j = 0; j < allignment; ++j) {
				bitmap_row<T> hrow(hbuffer[j]);
				bitmap_row<T> lrow(lbuffer[j]);
				this->core.exthbwd(hrow.ptr());
				this->core.rexthbwd(hrow.ptr() + hrow.width());
				this->core.extlbwd(lrow.ptr());
				this->core.rextlbwd(lrow.ptr() + lrow.width());
				this->core.bwd(hrow.ptr(), lrow.ptr(), dstbuffer[j].ptr(), buffer_width);
			}
			for (size_t ii = 0; ii < allignment; ++ii) {
				for (size_t jj = 0; jj < source_meta.width; ++jj) {
					if (source[i + ii][jj] != dstbuffer[ii][jj]) {
						throw "NEQ!";
					}
				}
			}
		}
		for (size_t k = 0; k < buffer_width; ++k) {
			bitmap_row<T> hrow = hdst[k];
			bitmap_row<T> lrow = ldst[k];
			for (size_t j = 0; j < allignment; ++j) {
				hrow[i + j] = hbuffer[j][k];
				lrow[i + j] = lbuffer[j][k];
			}
		}
	}
}

template <typename T, size_t allignment>
std::vector<block<T>> ForwardWaveletTransformer<T, allignment>::pack() {
	img_meta DC_loc = this->buffers[this->c_LL3_offset].getImgMeta();
	std::vector<block<T>> output(DC_loc.width * DC_loc.height);

	for (size_t i = 0; i < DC_loc.height; ++i) {
		for (size_t j = 0; j < DC_loc.width; ++j) {
			block<T>& current = output[i * DC_loc.width + j];
			current.content[0] = this->buffers[this->c_LL3_offset][i][j];
			for (size_t k = 0; k < this->c_families_count; ++k) {
				size_t offset = 1;
				size_t index = 0;
				size_t size = 1;
				size_t stride = 1;
				current.content[k + offset] = this->buffers[this->c_LL3_offset + k + 1][i][j];

				index = 0;
				offset += 3 * size;
				stride = 2;
				size = stride * stride;
				for (size_t ii = 0; ii < 2; ++ii) {
					for (size_t jj = 0; jj < 2; ++jj) {
						current.content[offset + k * size + index] =
							this->buffers[this->c_LL3_offset + k + 4][i * stride + ii][j * stride + jj];
						++index;
					}
				}

				index = 0;
				offset += 3 * size;
				stride = 4;
				size = stride * stride;
				for (size_t ii = 0; ii < 2; ++ii) {
					for (size_t jj = 0; jj < 2; ++jj) {
						for (size_t iii = 0; iii < 2; ++iii) {
							for (size_t jjj = 0; jjj < 2; ++jjj) {
								current.content[offset + k * size + index] =
									this->buffers[this->c_LL3_offset + k + 7]
									[i * stride + ii * 2 + iii][j * stride + jj * 2 + jjj];
								++index;
							}
						}
					}
				}
			}
		}
	}
	return output;
}

template <typename T, size_t allignment>
void ForwardWaveletTransformer<T, allignment>::ext(bitmap<T>& source, bitmap<T>& hdst, bitmap<T>& ldst, size_t i) {
	// Change this->m_src_meta to source.getImgMeta();
	img_meta source_meta = source.getImgMeta();
	size_t buffer_width = source_meta.width / 2;
	bitmap<T> lbuffer(buffer_width, allignment, 32);
	bitmap<T> hbuffer(buffer_width, allignment, 32);
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
	size_t ext_bounding = ((j + (this->c_v_allignment - 1)) & (~(this->c_v_allignment - 1)));
	for (; j < ext_bounding; ++j) {
		lbuffer[j].assign(lbuffer[last]);
		hbuffer[j].assign(hbuffer[last]);
	}
	for (; j < allignment; ++j) {
		lbuffer[j].assign(0);
		hbuffer[j].assign(0);
	}
	for (size_t k = 0; k < buffer_width; ++k) {
		bitmap_row<T> hrow = hdst[k];
		bitmap_row<T> lrow = ldst[k];
		for (j = 0; j < allignment; ++j) {
			hrow[base + j] = hbuffer[j][k];
			lrow[base + j] = lbuffer[j][k];
		}
	}
}

template <typename T, size_t allignment = 16>
class BackwardWaveletTransformer {
	std::array<typename bitmap<T>, 18> buffers;
	std::array<img_meta, 3> current_frames;
	dwtcore<T> core;

	std::array<bitmap<T>, 6> v_extension_buffers;

	img_meta m_src_meta;
	img_pos m_frame_pos;

	static const size_t c_h_allignment = 8;
	static const size_t c_v_allignment = 8;
	static const size_t c_level_count = 3;

	static const size_t c_families_count = 3;
	static const size_t c_LL3_offset = 8;

public:
	BackwardWaveletTransformer(img_pos frame_properties);

	// TODO: takes source image as input only
	// Image entry point moves transform frame accross the whole image
	// Manages additional buffers that depends on input image properties.
	bitmap<T> apply(const std::vector<block<T>>& input);
	bitmap<T> apply();

	auto _getBuffers() {
		return this->buffers;
	}

	void _setBuffers(decltype(buffers)& __buffers) {
		// this->buffers[2] = __buffers[2].transpose();
		// this->buffers[15] = __buffers[15].transpose();
		// this->buffers[16] = __buffers[16].transpose();
		// this->buffers[17] = __buffers[17].transpose();
		// 
		// this->buffers[0] = __buffers[0].transpose();
		// this->buffers[1] = __buffers[1].transpose();

		// this->buffers[8] = __buffers[8].transpose();
		// this->buffers[9] = __buffers[9].transpose();
		// this->buffers[10] = __buffers[10].transpose();
		// this->buffers[11] = __buffers[11].transpose();

		// Set all necessary planes from input blocks
		for (size_t i = 8; i < this->buffers.size(); ++i) {
			this->buffers[i] = __buffers[i].transpose();
		}

		constexpr size_t verify_index = 8;
		img_meta temp = this->buffers[verify_index].getImgMeta();
		for (size_t i = 0; i < temp.height; ++i) {
			for (size_t j = 0; j < temp.width; ++j) {
				if (this->buffers[verify_index][i][j] != __buffers[verify_index][j][i]) {
					throw "NEQ!";
				}
			}
		}
	}

	void _setPayloadBuffers(std::array<typename bitmap<T>, 10>& __buffers) {
		// this->buffers[2] = __buffers[2].transpose();
		// this->buffers[15] = __buffers[15].transpose();
		// this->buffers[16] = __buffers[16].transpose();
		// this->buffers[17] = __buffers[17].transpose();
		// 
		// this->buffers[0] = __buffers[0].transpose();
		// this->buffers[1] = __buffers[1].transpose();

		// this->buffers[8] = __buffers[8].transpose();
		// this->buffers[9] = __buffers[9].transpose();
		// this->buffers[10] = __buffers[10].transpose();
		// this->buffers[11] = __buffers[11].transpose();

		// Set all necessary planes from input blocks
		for (size_t i = 0; i < __buffers.size(); ++i) {
			if (this->c_LL3_offset + i >= this->buffers.size())
				throw "OOB!";
			this->buffers[this->c_LL3_offset + i] = __buffers[i].transpose();
		}
	}

private:
	void transform(const bitmap<T>& hsrc, const bitmap<T>& lsrc, bitmap<T>& dst);
	void unpack(const std::vector<block<T>>& input);
};

template <typename T, size_t allignment>
BackwardWaveletTransformer<T, allignment>::BackwardWaveletTransformer(img_pos frame_properties) {
	this->m_frame_pos.width = frame_properties.width;
	this->m_frame_pos.height = frame_properties.height;
	constexpr size_t buffer_iter_step = 3;

	size_t width = this->m_frame_pos.height;
	size_t height = this->m_frame_pos.width; // * 2;
	for (size_t i = this->c_level_count; i > 0; --i) {
		this->buffers[(i - 1) * buffer_iter_step + 2].resize(width, height);
		for (size_t j = 0; j < 3; ++j) {
			this->buffers[this->buffers.size() - i * buffer_iter_step + j].resize(width, height);
		}
		width ^= height;
		height ^= width;
		width ^= height;
		height *= 2;
		for (size_t j = 0; j < 2; ++j) {
			this->buffers[(i - 1) * buffer_iter_step + j].resize(width, height);
		}
		width ^= height;
		height ^= width;
		width ^= height;
		height *= 2;
	}
}

template <typename T, size_t allignment>
bitmap<T> BackwardWaveletTransformer<T, allignment>::apply(const std::vector<block<T>>& input) {
	this->unpack(input);
	size_t width = this->buffers[0].getImgMeta().height;
	size_t height = this->buffers[0].getImgMeta().width * 2;
	// TODO: fix rvalue reference
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
	// TODO: fix rvalue reference, so object is moved and not copied
	return dst.transpose();
}

template <typename T, size_t allignment>
bitmap<T> BackwardWaveletTransformer<T, allignment>::apply() {
	size_t width = this->buffers[0].getImgMeta().height;
	size_t height = this->buffers[0].getImgMeta().width * 2;
	// TODO: fix rvalue reference
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
	// TODO: fix rvalue reference, so object is moved and not copied
	return dst.transpose();
}

template <typename T, size_t allignment>
void BackwardWaveletTransformer<T, allignment>::transform(const bitmap<T>& hsrc, const bitmap<T>& lsrc, bitmap<T>& dst) {
	// Change this->m_src_meta to source.getImgMeta();
	img_meta source_meta = hsrc.getImgMeta();
	size_t buffer_width = source_meta.width * 2;
	bitmap<T> dstbuffer(buffer_width, allignment, 32);
	size_t i = 0;
	for (; i < source_meta.height; i += allignment) {
		for (size_t j = 0; j < allignment; ++j) {
			bitmap_row<T> hrow(hsrc[i + j]);
			bitmap_row<T> lrow(lsrc[i + j]);
			this->core.exthbwd(hrow.ptr());
			this->core.rexthbwd(hrow.ptr() + hrow.width());
			this->core.extlbwd(lrow.ptr());
			this->core.rextlbwd(lrow.ptr() + lrow.width());
			this->core.bwd(hrow.ptr(), lrow.ptr(), dstbuffer[j].ptr(), buffer_width / 2);
		}
		for (size_t k = 0; k < buffer_width; ++k) {
			bitmap_row<T> dstrow = dst[k];
			for (size_t j = 0; j < allignment; ++j) {
				dstrow[i + j] = dstbuffer[j][k];
			}
		}
	}
}

template <typename T, size_t allignment>
void BackwardWaveletTransformer<T, allignment>::unpack(const std::vector<block<T>>& input) {
	img_meta DC_loc = this->buffers[this->c_LL3_offset].getImgMeta();
	if ((DC_loc.width * DC_loc.height) != input.size()) {
		throw "NEQ!";
	}

	for (size_t i = 0; i < DC_loc.width; ++i) {
		for (size_t j = 0; j < DC_loc.height; ++j) {
			bool hitmap[64] = { false };
			const block<T>& current = input[i * DC_loc.height + j];
			this->buffers[this->c_LL3_offset][j][i] = current.content[0];
			hitmap[0] = true;
			for (size_t k = 0; k < this->c_families_count; ++k) {
				size_t offset = 1;
				size_t index = 0;
				size_t size = 1;
				size_t stride = 1;
				this->buffers[this->c_LL3_offset + k + 1][j][i] = current.content[k + offset];
				hitmap[k + offset] = true;

				index = 0;
				offset += 3 * size;
				stride = 2;
				size = stride * stride;
				for (size_t ii = 0; ii < 2; ++ii) {
					for (size_t jj = 0; jj < 2; ++jj) {
						this->buffers[this->c_LL3_offset + k + 4][j * stride + jj][i * stride + ii] =
							current.content[offset + k * size + index];
						hitmap[offset + k * size + index] = true;
						++index;
					}
				}

				index = 0;
				offset += 3 * size;
				stride = 4;
				size = stride * stride;
				for (size_t ii = 0; ii < 2; ++ii) {
					for (size_t jj = 0; jj < 2; ++jj) {
						for (size_t iii = 0; iii < 2; ++iii) {
							for (size_t jjj = 0; jjj < 2; ++jjj) {
								this->buffers[this->c_LL3_offset + k + 7]
									[j * stride + jj * 2 + jjj][i * stride + ii * 2 + iii] =
									current.content[offset + k * size + index];
								hitmap[offset + k * size + index] = true;
								++index;
							}
						}
					}
				}
			}
		}
	}
}
