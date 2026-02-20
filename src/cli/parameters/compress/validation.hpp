#pragma once

#include "parameters/validation.hpp"
#include "parameters/compress/parameters.hpp"

namespace cli::validate::compress {

namespace params = parameters::compress;

validation_context validate_parameters(const params::compress_command& parameters);

}
