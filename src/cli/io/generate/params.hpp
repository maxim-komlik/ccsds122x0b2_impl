#pragma once

#include "io/image.hpp"
#include "parameters/compress/parameters.hpp"

namespace cli::io::generate {

image_description get_description(const parameters::compress::generate::generator& parameters) {
	return {
		.width = parameters.dims.width,
		.height = parameters.dims.height,
		.channel_num = parameters.dims.depth,
		.static_bdepth = parameters.bdepth,
		.if_signed = parameters.pixel_signed
	};
}

}
