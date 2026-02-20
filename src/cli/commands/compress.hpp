#pragma once

#include "cli.hpp"
#include "parameters/compress/parameters.hpp"

namespace cli::command::compress {

	namespace params = cli::parameters::compress;

	void compress_command_handler(const params::compress_command& parameters);

}
