#pragma once

#include <string_view>
#include <vector>

#include "cli.hpp"
#include "expected.hpp"

namespace cli::parsers {

struct flag_parser {
	using value_t = bool;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		return true;
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = ""sv;
};

// any ideas how to handle negated --no flags? anyway a flag is always named parameter, therefore may be optional
}
