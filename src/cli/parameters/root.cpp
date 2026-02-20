#include "root.hpp"

#include <optional>

#include "cli.hpp"
#include "parameters_context.hpp"
#include "contextual_parser.tpp"
#include "parsers/command_parser.hpp"

#include "parameters.hpp"
#include "compress.hpp"
#include "restore.hpp"

namespace cli::parameters {

template <>
struct parameter_context<command_line_parameters> : public parameter_context_default {
	static constexpr command<compress::compress_parser::value_t> default_compress = { std::nullopt };
	static constexpr command<restore::restore_parser::value_t> default_restore = { std::nullopt };

	static constexpr named_parameters_description_t named{
		parameter_description<parsers::command_parser<compress::compress_parser>>{"--compress", {default_compress}, "Compress input image"}, 
		parameter_description<parsers::command_parser<restore::restore_parser>>{"--restore", {default_restore}, "Restore compressed image"}
	};
};

root_parser::value_t root_parser::parse(std::vector<std::string_view>& tokens) {
	using parser_t = contextual_parser<value_t>;
	parser_t cx_parser;
	cx_parser.parse(tokens);

	return value_t{
		cx_parser.get<parser_t::name_to_index("--compress"sv)>(), 
		cx_parser.get<parser_t::name_to_index("--restore"sv)>()
	};
}

}
