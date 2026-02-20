#pragma once

#include <utility>
#include <cstddef>

#include "constant.hpp"

constexpr std::pair<size_t, size_t> padded_image_dimensions(size_t width, size_t height) noexcept {
	auto pad_dim = [](size_t dim) constexpr noexcept -> size_t {
		return (dim + constants::dwt::image_dimension_padding_mask) & 
			(~constants::dwt::image_dimension_padding_mask);
	};

	return { pad_dim(width), pad_dim(height) };
};

constexpr bool if_frame_dimensions_valid(size_t width, size_t height) noexcept {
	// validation staff, meant to be managed in interfaces. keep?
	bool result = true;
	result &= (width >= constants::img::min_width);
	result &= (height >= constants::img::min_height);
	result &= ((width & constants::dwt::frame_dimension_granularity_mask) == 0);
	result &= ((height & constants::dwt::frame_dimension_granularity_mask) == 0);

	return result;
}


// // TODO: check utility below if needed/should be moved to another location
// constexpr size_t block_index_to_buf_index(size_t l) {
// 	size_t bound_i = std::bit_width(l);
// 	bound_i += bound_i & 0x01;
// 	ptrdiff_t level = bound_i >> 1;
// 	ptrdiff_t mask = (0b11 << bound_i) >> 2;
// 	ptrdiff_t disp = (l & mask) >> (relu(level - 1) << 1);
// 
// 	return (3 * relu(level - 1) + disp);
// }
