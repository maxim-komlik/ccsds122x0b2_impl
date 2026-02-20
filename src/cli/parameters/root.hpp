#pragma once

#include "string_view"
#include "vector"

#include "cli.hpp"
#include "parameters.hpp"

namespace cli::parameters {

struct root_parser {
	using value_t = command_line_parameters;

	static value_t parse(std::vector<std::string_view>& tokens);
};

}
