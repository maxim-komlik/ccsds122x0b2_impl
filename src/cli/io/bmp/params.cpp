#include "params.hpp"

#include <span>
#include <algorithm>
#include <bit>

#include "io/file_cache.hpp"

namespace cli::io::bmp {

namespace {
	bmp_header_protocol parse_header(ibitwrapper<size_t>& src) {
		std::vector<std::byte> buffer(bmp_header_protocol::header_preamble_size);

		{
			std::span preable_static_view = std::span(buffer).first<bmp_header_protocol::header_preamble_size>();
			src.read_bytes(preable_static_view);

			size_t header_size_hint = bmp_header_protocol::hint_header_size(preable_static_view);

			// TODO: but that is actually guaranteed by hint_header_size, really need to check here?
			bool valid = true;
			valid &= (header_size_hint > bmp_header_protocol::header_preamble_size);

			if (!valid) {

			}

			buffer.resize(header_size_hint);
		}

		src.read_bytes(std::span(buffer).subspan(bmp_header_protocol::header_preamble_size));
		return bmp_header_protocol(buffer);
	}
}

image_description get_description(const parameters::compress::image_file& parameters) {
	auto& descriptor = file_cache_instance.get_descriptor(parameters.path);
	file_cache_value& data = file_cache_instance.get_data(descriptor);
	
	if (!std::holds_alternative<bmp_header_protocol>(data.file_protocol)) {
		data.src.setup_session();
		data.file_protocol = parse_header(data.src.get_bitwrapper());
	}

	bmp_header_protocol& protocol = std::get<bmp_header_protocol>(data.file_protocol);
	img_meta dimensions = protocol.get_image_dimensions();

	auto masks = protocol.get_channel_masks();
	std::span significant_masks = std::span(masks).first<std::tuple_size_v<decltype(masks)> - 1>();
	auto max_it = std::max_element(significant_masks.begin(), significant_masks.end(),
		[](uint32_t lhs, uint32_t rhs) -> bool { return std::popcount(lhs) < std::popcount(rhs); });
	size_t largest_channel_bdepth = std::popcount(*max_it);

	return image_description{
		.width = dimensions.width,
		.height = dimensions.height,
		.channel_num = dimensions.depth,
		.static_bdepth = largest_channel_bdepth,
		.if_signed = false	// bmp pixels are never signed
	};
}

}
