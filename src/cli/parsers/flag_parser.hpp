#pragma once

#include <string_view>
#include <vector>

#include "expected.hpp"

struct flag_parser {
	using value_t = bool;

	cli::expected<bool> parse(std::vector<std::string_view>& tokens) {
		return true;
	}

public:
	static constexpr std::string_view requirements = {};
	static constexpr std::string_view placeholder = {};
};

// any ideas how to handle negated --no flags? anyway a flag is always named parameter, therefore may be optional
