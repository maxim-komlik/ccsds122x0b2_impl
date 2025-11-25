#pragma once

struct img_meta {	// per-element location
	size_t stride;
	size_t offset;
	size_t width;
	size_t height;
	size_t depth;
	size_t length;
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
