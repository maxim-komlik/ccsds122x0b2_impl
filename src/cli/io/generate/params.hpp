#pragma once

#include "io/image.hpp"
#include "parameters/compress/parameters.hpp"

namespace cli::io::generate {

img_meta get_description(const parameters::compress::generate::generator& parameters) {
	return {
		.width = parameters.dims.width,
		.height = parameters.dims.height,
		.depth = parameters.dims.depth,
		.bdepth_static = parameters.bdepth,
		.if_signed = parameters.pixel_signed
	};
}

}
