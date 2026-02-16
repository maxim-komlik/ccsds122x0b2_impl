#pragma once

#include "compress.hpp"
#include "parameters_context.hpp"
#include "parsers/command_parser.hpp"

namespace cli::parameters {

using namespace std::literals;

struct cmd_parameters {
	command<compress::compress_parser> compress_cx;
};

template <>
struct parameter_context<cmd_parameters> : public parameter_context_default {
	static constexpr command<compress::compress_parser> default_compress = { std::nullopt };

	static constexpr named_parameters_description_t named{
		parameter_description<command_parser<compress::compress_parser>>{"--compress", {default_compress}, "Compress input image"}
	};
};

struct cmd_parser {
	using value_t = cmd_parameters;

	static cmd_parameters parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		return value_t{
			cx_parser.get<0>()
		};
	}
};

}
