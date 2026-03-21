#pragma once

#include <vector>
#include <utility>

#include "dwt/bitmap.tpp"

#include "header_protocol.hpp"
#include "io/file_cache.hpp"

namespace cli::io::bmp {

template <typename T>
std::vector<bitmap<T>> load_channels(const bmp_header_protocol& header_protocol, file_source<size_t>& src, size_t row_offset) {
	// TODO: better have offset as parameter for consistency

	src.seek(header_protocol.get_image_data_offset());
	img_pos scan_spec = header_protocol.get_image_scan_spec();

	bool valid = true;
	valid &= (true);

	if (!valid) {
		// TODO: error handling
	}

	std::vector<bitmap<T>> result;
	result.reserve(scan_spec.depth);

	for (ptrdiff_t i = 0; i < scan_spec.depth; ++i) {
		result.emplace_back(scan_spec.width, scan_spec.height, row_offset);
	}

	auto& input = src.get_bitwrapper();
	input.set_byte_limit(header_protocol.file_size());

	size_t bits_per_pixel = header_protocol.get_pixel_bit_size();
	auto channel_masks = header_protocol.get_channel_masks();

	for (ptrdiff_t y = scan_spec.y; (y < scan_spec.height) & (y >= 0); y += scan_spec.y_step) {
		for (ptrdiff_t x = scan_spec.x; (x < scan_spec.width) & (x >= 0); x += scan_spec.x_step) {
			uint32_t pixel = input.extract(bits_per_pixel);
			for (ptrdiff_t z = 0; z < scan_spec.depth; ++z) {
				result[z][y][x] = (pixel & channel_masks[z]) >> std::countr_zero(channel_masks[z]);
			}
		}
		
		// bmp alignes every row on 4-byte boundary, counting from the image array beginning
		constexpr size_t alignment_requirement = 4;
		size_t bits_extracted = (input.get_byte_count() << 3) - input.get_buffer_bit_width();
		size_t aligned_offset = (((bits_extracted + ((1 << 3) - 1)) >> 3) + (alignment_requirement - 1)) & 
			(~(alignment_requirement - 1));

		input.extract((aligned_offset << 3) - bits_extracted);
	}

	return result;
}

template <typename T>
bitmap<T> load_channel(const file_descriptor& descriptor, ptrdiff_t index, size_t row_offset = 16) {
	using value_t = std::vector<bitmap<T>>;

	auto& cached = file_cache_instance.get_data(descriptor);
	if (!cached.content_data.has_value()) {
		const auto& protocol = std::get<bmp_header_protocol>(cached.file_protocol);
		cached.content_data = load_channels<T>(protocol, cached.src, row_offset);
	}

	auto& channels = std::any_cast<value_t&>(cached.content_data);

	bool valid = true;
	valid &= (index < channels.size());

	if (!valid) {
		// TODO: error handling
	}

	// TODO: load is consume operation, multiple calls on same channel is not intended?
	return std::move(channels[index]);
}

}
