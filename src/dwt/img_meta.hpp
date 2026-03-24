#pragma once

#include <cstddef>

struct img_meta {
	size_t width = 0;
	size_t height = 0;
	size_t depth = 0;
	size_t bdepth_static = 0;
	size_t offset_requirement = 0;
	size_t alignment_requirement = 0;
	bool if_signed = false;
};


struct img_pos {
	ptrdiff_t x = 0;
	ptrdiff_t x_step = 0;
	ptrdiff_t y = 0;
	ptrdiff_t y_step = 0;
	ptrdiff_t z = 0;
	ptrdiff_t z_step = 0;
	size_t x_stride = 0;
	size_t width = 0;
	size_t y_stride = 0;
	size_t height = 0;
	size_t z_stride = 0;
	size_t depth = 0;

	img_pos transpose() {
		img_pos result = *this;

		result.x ^= result.y;
		result.y ^= result.x;
		result.x ^= result.y;
		result.width ^= result.height;
		result.height ^= result.width;
		result.width ^= result.height;

		result.x_stride ^= result.y_stride;
		result.y_stride ^= result.x_stride;
		result.x_stride ^= result.y_stride;

		// TODO: stride values?
		return result;
	}
};
