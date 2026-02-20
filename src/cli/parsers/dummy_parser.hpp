#pragma once

#include <string_view>
#include <vector>

#include "cli.hpp"
#include "expected.hpp"

namespace cli::parsers {

struct dummy_parser {
	struct parameter {};

	using value_t = parameter;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) { return value_t{}; }
	static constexpr parameter make_default() { return value_t{}; }

	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<dependent>"sv;

	using default_representation_t = value_t;
};

}
