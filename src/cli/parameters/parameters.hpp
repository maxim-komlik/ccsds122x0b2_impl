#pragma once

#include "parsers/command_parser.hpp"

#include "compress/parameters.hpp"
#include "restore/parameters.hpp"
#include "validation.hpp"

namespace cli::parameters {

struct command_line_parameters {
	command<compress::compress_command> compress_cx;
	command<restore::restore_command> restore_cx;

	bool if_empty() const;
	validate::validation_context validate() const;
	void execute() const;

};

}
