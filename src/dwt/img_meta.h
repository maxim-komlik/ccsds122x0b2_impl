#pragma once

struct img_meta {	// per-element location
	size_t stride = 0;
	size_t offset = 0;
	size_t width = 0;
	size_t height = 0;
	size_t depth = 0;
	size_t length = 0;
};


struct img_pos {
	size_t x = 0;
	size_t x_step = 0;
	size_t y = 0;
	size_t y_step = 0;
	size_t z = 0;
	size_t z_step = 0;
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

		// TODO: stride values?
		return result;
	}
};
