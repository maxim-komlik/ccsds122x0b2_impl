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

img_meta get_description(const parameters::compress::image_file& parameters) {
	auto& descriptor = file_cache_instance.get_descriptor(parameters.path);
	file_cache_value& data = file_cache_instance.get_data(descriptor);
	
	if (!std::holds_alternative<bmp_header_protocol>(data.file_protocol)) {
		data.src.setup_session();
		data.file_protocol = parse_header(data.src.get_bitwrapper());
	}

	bmp_header_protocol& protocol = std::get<bmp_header_protocol>(data.file_protocol);
	return protocol.get_image_description();
}

}
