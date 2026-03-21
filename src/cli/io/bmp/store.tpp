#pragma once

#include <array>
#include <vector>
#include <bit>
#include <filesystem>
#include <cstddef>

#include "dwt/bitmap.tpp"
#include "io/io_data_registry.hpp"
#include "io/file_sink.tpp"
#include "io/file_cache.hpp"

#include "header_protocol.hpp"

namespace cli::io::bmp {

template <typename T>
void store_image(const std::vector<bitmap<T>>& img, const std::filesystem::path& dst_path) {
	bool valid = true;
	valid &= !std::filesystem::exists(dst_path);

	if (!valid) {
		// TODO: error handling
	}

	bmp_header_protocol protocol(img);

	external_file_descriptor descriptor(dst_path);

	{
		file_sink<size_t> sink(descriptor);
		sink.setup_session();

		std::array<std::byte, bmp_header_protocol::max_header_size()> header_storage{};
		auto& output = sink.get_bitwrapper();
		output.set_byte_limit(protocol.file_size());

		output.write_bytes(protocol.commit(header_storage));

		// assume channels ordered continiously, first channel MSb
		auto channel_masks = protocol.get_channel_masks();
		std::rotate(channel_masks.rbegin(), std::next(channel_masks.rbegin()), channel_masks.rend()); // move X mask to the beginning
		std::array<size_t, std::tuple_size_v<decltype(channel_masks)>> channel_bdepths;
		for (ptrdiff_t i = 0; i < channel_masks.size(); ++i) {
			channel_bdepths[i] = std::popcount(channel_masks[i]);
		}

		{
			size_t byte_aligned = (output.get_buffer_bit_width() + ((1 << 3) - 1)) & (~((1 << 3) - 1));
			output << vlw_t{ byte_aligned - output.get_buffer_bit_width(), 0u};
		}
	
		size_t img_array_offset = output.get_byte_count() + (output.get_buffer_bit_width() >> 3);

		img_pos scan_spec = protocol.get_image_scan_spec();
		for (ptrdiff_t y = scan_spec.y; (y < scan_spec.height) & (y >= 0); y += scan_spec.y_step) {
			for (ptrdiff_t x = scan_spec.x; (x < scan_spec.width) & (x >= 0); x += scan_spec.x_step) {
				constexpr size_t subpixels_size = std::tuple_size_v<decltype(channel_bdepths)>;
				std::array<T, subpixels_size> subpixels{ 0 };
				for (ptrdiff_t i = 0; i < scan_spec.depth; ++i) {
					subpixels[i + 1] = img[i][y][x];	// first subpixels item is X mask fill value
				}

				for (ptrdiff_t i = 0; i < subpixels_size; ++i) {
					output << vlw_t{ channel_bdepths[i], (vlw_t::type)(subpixels[i]) };
				}
			}

			// bmp alignes every row on 4-byte boundary, counting from the image array beginning
			constexpr size_t alignment_requirement = 4;
			size_t bits_written = (output.get_byte_count() << 3) + output.get_buffer_bit_width();
			bits_written -= img_array_offset << 3;
			size_t aligned_offset = (((bits_written + ((1 << 3) - 1)) >> 3) 
				+ (alignment_requirement - 1)) & (~(alignment_requirement - 1));
			aligned_offset <<= 3;

			output << vlw_t{ aligned_offset - bits_written, 0u };
		}

		size_t written_file_size = output.get_byte_count() + (output.get_buffer_bit_width() >> 3);

		valid &= written_file_size == protocol.file_size();
		if (!valid) {
			// TODO: error handling 
		}
	}

	std::filesystem::resize_file(dst_path, protocol.file_size());
}

}
