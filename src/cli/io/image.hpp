#pragma once

#include <vector>
#include <functional>

#include "dwt/img_meta.hpp"
#include "io/io_data_registry.hpp"

#include "cli.hpp"
#include "parameters/compress/parameters.hpp"
#include "parameters/restore/parameters.hpp"

namespace cli::io {

struct image_load_parameters {
	parameters::compress::source cli_parameters;
	img_meta meta;
};

struct image_store_parameters {
	parameters::restore::destination cli_parameters;
	img_meta meta;
};

image_load_parameters get_image_description(const parameters::compress::source& parameters);
image_load_parameters get_image_description(parameters::compress::source&& parameters);


std::vector<std::reference_wrapper<const data_descriptor>> import_image(
	io_data_registry& registry, const io::image_load_parameters& import_specs);

void export_image(io_data_registry&& registry, const io::image_store_parameters& export_specs);

}
