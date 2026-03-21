#pragma once

#include "io/image.hpp"
#include "parameters/compress/parameters.hpp"

namespace cli::io::bmp {

img_meta get_description(const parameters::compress::image_file& parameters);

}
