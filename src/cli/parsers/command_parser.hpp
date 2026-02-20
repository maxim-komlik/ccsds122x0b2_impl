#pragma once

#include <string_view>
#include <vector>
#include <optional>

#include "cli.hpp"
#include "expected.hpp"
#include "utility.hpp"

template <typename CommandParamsParser>
struct command;

// default value string representation placeholder, because by default the command is not specified
template <>
struct command<void> { 
	const bool selected = false;
};

template <typename T>
struct command {
	using value_t = T;

	std::optional<value_t> context;

	constexpr operator command<void>() { return {}; }
};

namespace cli::parsers {

template <typename CommandParamsParser>
struct command_parser {
	using value_t = command<typename CommandParamsParser::value_t>;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		return value_t{
			std::make_optional<CommandParamsParser::value_t>(CommandParamsParser().parse(tokens).value())
		};
	}

private:
	static consteval std::string_view generate_placeholder() {
		using namespace std::literals;
		return "<command>"sv;
	}

public:
	static constexpr std::string_view requirements = {};
	static constexpr std::string_view placeholder = generate_placeholder();

	using default_representation_t = command<void>;
};

}

namespace meta {

	template <>
	struct member_names_trait<command<void>> {
		static constexpr member_names_description_t names = {
			"selected"sv
		};
	};
}
