#pragma once

#include "parameters/validation.hpp"
#include "parameters/restore/parameters.hpp"

namespace cli::validate::restore {

namespace params = parameters::restore;

validation_context validate_parameters(const params::restore_command& parameters);

}
