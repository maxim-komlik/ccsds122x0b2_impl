#pragma once

#include "cli.hpp"
#include "parameters/restore/parameters.hpp"

namespace cli::command::restore {

	namespace params = cli::parameters::restore;

	void restore_command_handler(const params::restore_command& parameters);

}
