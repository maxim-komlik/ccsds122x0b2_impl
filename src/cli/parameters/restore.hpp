#pragma once

#include <string_view>
#include <vector>
#include <functional>

#include "parameters_context.hpp"
#include "contextual_parser.tpp"
#include "parsers/flag_parser.hpp"
#include "parsers/integer_parser.hpp"
#include "parsers/enum_parser.hpp"
#include "parsers/path_parser.hpp"
#include "utility.hpp"

#include "restore/parameters.hpp"

namespace cli::parameters::restore {

using namespace cli::parsers;

	
template <>
struct parameter_context<file_sink_params> : public parameter_context_default {
	static constexpr named_parameters_description_t named{
		parameter_description<path_parser<false, true>>{"--path"sv, {}, "Compressed segments filsystem location"sv}
	};
};

template <>
struct parameter_context<source> : public parameter_context_default {
	static constexpr immediate_parameters_description_t immediates{
		parameter_description<enum_parser<src_type>>{{}, {src_type::file}, "Compressed segments source storage type"sv}
	};

	static constexpr named_parameters_description_t named{
		parameter_description<enum_parser<segment_protocol_type>>{"--protocol"sv, {segment_protocol_type::detect}, "Protocol headers type to use to decode segments"sv}, 
		// TODD: as is below breaks parsing for special tokens. move hint parameters to dedicated member?
		dynamic_parameter{"Source storage type dependent parameters"sv}.expand()
	};
};

struct source_parser {
	using value_t = source;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		using sink_parser_t = contextual_parser<file_sink_params>;
		sink_parser_t sink_parser;
		sink_parser.parse(tokens);

		return value_t{
			cx_parser.get<0>(),
			cx_parser.get<parser_t::name_to_index("--protocol"sv)>(),
			file_sink_params{ sink_parser.get<sink_parser_t::name_to_index("--path"sv)>() }
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<src_params>"sv;
};


template <>
struct parameter_context<destination> : public parameter_context_default {
	static constexpr immediate_parameters_description_t immediates{
		parameter_description<enum_parser<dst_type>>{{}, {dst_type::memory}, "Restored image storage type"sv}
	};

	static constexpr named_parameters_description_t named{
		parameter_description<enum_parser<image_protocol_type>>{"--protocol"sv, {image_protocol_type::raw}, "Format to use to store the image"sv}
	};
};

struct destination_parser {
	using value_t = destination;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		return value_t{
			cx_parser.get<0>(),
			cx_parser.get<parser_t::name_to_index("--protocol"sv)>()
		};
	}

	static consteval value_t make_default() {
		using parser_t = contextual_parser<value_t>;

		return value_t{
			parser_t::get_default<0>(),
			parser_t::get_default<parser_t::name_to_index("--protocol"sv)>()
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<dst_params>"sv;

	using default_representation_t = value_t;
};


template <>
struct parameter_context<stream::parameters_set> : public parameter_context_default {
	static constexpr named_parameters_description_t named{
		parameter_description<unsigned_integer_parser<>>{"--id"sv, {}, "First segment ID to apply setting to"sv},
		parameter_description<unsigned_integer_parser<>>{"--id-for"sv, {0}, "Number of consequtive segments to apply setting to"sv},
		parameter_description<unsigned_integer_parser<>>{"--id-until"sv, {0}, "Last segment ID to apply setting to"sv},
		parameter_description<unsigned_integer_parser<(1 << 27), 8>>{"--limit"sv, {1 << 27}, "Stream byte size to stop segment decoding at"sv},
		parameter_description<unsigned_integer_parser<31>>{"--stop-bitplane"sv, {0}, "Bit plane index to stop decoding at"sv},
		parameter_description<unsigned_integer_parser<4>>{"--stop-stage"sv, {4}, "Bit plane encoding stage number to stop decoding at"sv},
		parameter_description<flag_parser>{"--stop-dc"sv, {false}, "Indicates to stop segment decoding after quantized DC coefficients are decoded"sv}
	};
};

struct stream_parser {
	using value_t = stream;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		// TODO: variable length arguments parsing needed here
		using parser_t = contextual_parser<stream::parameters_set>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		return value_t{
			{
				cx_parser.get<parser_t::name_to_index("--id"sv)>(),
				cx_parser.get<parser_t::name_to_index("--limit"sv)>(),
				cx_parser.get<parser_t::name_to_index("--stop-bitplane"sv)>(),
				cx_parser.get<parser_t::name_to_index("--stop-stage"sv)>(),
				cx_parser.get<parser_t::name_to_index("--stop-dc"sv)>()
			}
		};
	}

	static consteval value_t make_default() {
		using parser_t = contextual_parser<stream::parameters_set>;

		constexpr size_t first_id = 0;
		return value_t{
			{
				first_id,
				parser_t::get_default<parser_t::name_to_index("--limit"sv)>(),
				parser_t::get_default<parser_t::name_to_index("--stop-bitplane"sv)>(),
				parser_t::get_default<parser_t::name_to_index("--stop-stage"sv)>(),
				parser_t::get_default<parser_t::name_to_index("--stop-dc"sv)>()
			}
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<stream_params>"sv;

	using default_representation_t = stream::parameters_set;
};


template <>
struct parameter_context<restore_command> : public parameter_context_default {
	static constexpr stream default_stream = stream_parser::make_default();
	static constexpr destination default_dst = destination_parser::make_default();

	static constexpr named_parameters_description_t named{
		parameter_description<source_parser>{"--src"sv, {}, "Specifies input settings"sv},
		parameter_description<destination_parser>{"--dst"sv, {default_dst}, "Specifies output settings"sv},
		parameter_description<stream_parser>{"--stream"sv, {default_stream}, "Specifies stream bitrate settings, reducing restored image quality"sv},
		parameter_description<flag_parser>{"--force-transpose"sv, {false}, "Transpose output image after processing"sv}
	};
};

struct restore_parser {
	using value_t = restore_command;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		return value_t{
			cx_parser.get<parser_t::name_to_index("--src"sv)>(),
			cx_parser.get<parser_t::name_to_index("--dst"sv)>(),
			cx_parser.get<parser_t::name_to_index("--stream"sv)>()
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<restore_params>"sv;
};

}