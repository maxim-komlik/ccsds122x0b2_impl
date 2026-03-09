#pragma once

#include <string_view>
#include <tuple>
#include <optional>

#include "cli.hpp"
#include "parsers/flag_parser.hpp"
#include "parsers/dummy_parser.hpp"

namespace cli::parameters {

template <typename Parser>
struct parameter_description {
	using parser_t = Parser;
	using value_t = typename Parser::value_t;

	std::string_view name;
	std::optional<value_t> default_value;

	std::string_view help_description;
};

template <typename... ImmediateParsers>
using immediate_parameters_description_t = std::tuple<parameter_description<ImmediateParsers>...>;

template <typename... NamedParsers>
using named_parameters_description_t = std::tuple<parameter_description<NamedParsers>...>;


struct parameter_context_default {
	static constexpr immediate_parameters_description_t immediates{};
	static constexpr named_parameters_description_t named{};
	static constexpr named_parameters_description_t variable_length_arguments{}; // TODO: underlying type?
};

template <typename OptionT>
struct parameter_context;


struct dynamic_parameter {
	using value_t = cli::parsers::dummy_parser::parameter;

	std::string_view help_description;

	constexpr parameter_description<cli::parsers::dummy_parser> expand() const {
		return parameter_description<cli::parsers::dummy_parser>{{}, cli::parsers::dummy_parser::make_default(), help_description};
	}
};

struct global_context {
	static constexpr parameter_description<parsers::flag_parser> help{ "--help"sv, {}, "Displays description for usage of commands and parameters"sv };
	static constexpr parameter_description<parsers::flag_parser> context_exit{ "--"sv, {}, "Indicates the end of parameters for the most nested parsing context"sv };
};

}
