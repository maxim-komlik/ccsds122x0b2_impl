#pragma once

#include <string_view>
#include <vector>
#include <functional>

#include "parameters_context.hpp"
#include "contextual_parser.tpp"
#include "parsers/flag_parser.hpp"
#include "parsers/integer_parser.hpp"
#include "parsers/enum_parser.hpp"

#include "compress/parameters.hpp"


namespace cli::parameters {

namespace compress {

	namespace generate {
		struct image_dimensions_parser;
		struct generator_parser;
	}
	
	struct source_parser;
	struct destination_parser;
	struct shifts_parser;
	struct frame_dimensions_parser;
	struct dwt_parser;
	struct segment_parser;
	struct stream_parser;
	struct compress_parser;

}


using namespace cli::parsers;

template <>
struct parameter_context<compress::generate::image_dimensions> : public parameter_context_default {
	static constexpr immediate_parameters_description_t immediates {
		parameter_description<unsigned_integer_parser<(1 << 20), 17>>{{}, {}, "Target image width"sv},
		parameter_description<unsigned_integer_parser<std::numeric_limits<ptrdiff_t>::max(), 17>>{{}, {}, "Target image height"sv},
		parameter_description<unsigned_integer_parser<(1 << 8), 1>>{ {}, {1}, "Target image number of channels"sv }
	};
};

struct compress::generate::image_dimensions_parser {
	using value_t = image_dimensions;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		contextual_parser<value_t> cx_parser;
		cx_parser.parse(tokens);

		return value_t{
			cx_parser.get<0>(), 
			cx_parser.get<1>(), 
			cx_parser.get<2>()
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<dims>"sv;
};


template <>
struct parameter_context<compress::generate::generator> : public parameter_context_default {
	static constexpr immediate_parameters_description_t immediates {
		parameter_description<compress::generate::image_dimensions_parser>{{}, {}, "Target image dimensions"sv}
	};

	static constexpr named_parameters_description_t named {
		parameter_description<integer_parser<>>{"--seed"sv, {1067}, "Image pixel values generator seed"sv},
		parameter_description<unsigned_integer_parser<28>>{"--bdepth"sv, {20}, "Image pixel depth"sv},
		parameter_description<flag_parser>{"--signed"sv, {false}, "Generate signed pixel values for image"sv}
	};
};

struct compress::generate::generator_parser {
	using value_t = generator;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		return value_t{
			cx_parser.get<0>(),
			cx_parser.get<parser_t::name_to_index("--bdepth"sv)>(),
			cx_parser.get<parser_t::name_to_index("--signed"sv)>(),
			cx_parser.get<parser_t::name_to_index("--seed"sv)>()
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<gen_params>"sv;
};


template <>
struct parameter_context<compress::source> : public parameter_context_default {
	static constexpr immediate_parameters_description_t immediates{
		parameter_description<enum_parser<compress::src_type>>{{}, {}, "Image source type"sv}
	};
};

struct compress::source_parser {
	using value_t = source;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		const src_type& type_value = cx_parser.get<0>();

		switch (type_value) {
		case src_type::generate: {
			cli::expected<generate::generator> generation_params = generate::generator_parser().parse(tokens);
			if (!generation_params) {

			}

			return value_t{
				type_value,
				generation_params.value()
			};
		}
		default: {
			// TODO: C++23 std::unreachable
		}
		}
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<src_params>"sv;
};


template <>
struct parameter_context<compress::destination> : public parameter_context_default {
	static constexpr immediate_parameters_description_t immediates{
		parameter_description<enum_parser<compress::dst_type>>{{}, {}, "Compressed segments destination type"sv}
	};

	static constexpr named_parameters_description_t named{
		parameter_description<enum_parser<compress::protocol_type>>{"--protocol"sv, {compress::protocol_type::standard}, "Protocol headers set to use in encoded segments"sv}
	};
};

struct compress::destination_parser {
	using value_t = destination;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		const dst_type& type_value = cx_parser.get<0>();
		switch (type_value) {
		case dst_type::file: {
			protocol_type protocol_value = cx_parser.get<parser_t::name_to_index("--protocol"sv)>();
			if (cx_parser.if_default<parser_t::name_to_index("--protocol"sv)>()) {
				protocol_value = protocol_type::file;
			}

			return value_t{
				type_value,
				protocol_value,
				file_sink_params{}
			};
		}
		case dst_type::memory: {
			protocol_type protocol_value = cx_parser.get<parser_t::name_to_index("--protocol"sv)>();
			if (cx_parser.if_default<parser_t::name_to_index("--protocol"sv)>()) {
				protocol_value = protocol_type::memory;
			}

			return value_t{
				type_value,
				protocol_value,
				memory_sink_params{}
			};
		}
		default: {
			// C++23 std::unreachable
		}
		}
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<dst_params>"sv;
};


template <>
struct parameter_context<compress::shifts> : public parameter_context_default {
	using shift_value_parser = unsigned_integer_parser<3>;

	static constexpr immediate_parameters_description_t immediates {
		// 10 integers for every subband weight parameter
		parameter_description<shift_value_parser>{{}, {}, "LL3 weight"sv},
		parameter_description<shift_value_parser>{{}, {}, "HL3 weight"sv},
		parameter_description<shift_value_parser>{{}, {}, "LH3 weight"sv},
		parameter_description<shift_value_parser>{{}, {}, "HH3 weight"sv},
		parameter_description<shift_value_parser>{{}, {}, "HL2 weight"sv},
		parameter_description<shift_value_parser>{{}, {}, "LH2 weight"sv},
		parameter_description<shift_value_parser>{{}, {}, "HH2 weight"sv},
		parameter_description<shift_value_parser>{{}, {}, "HL1 weight"sv},
		parameter_description<shift_value_parser>{{}, {}, "LH1 weight"sv},
		parameter_description<shift_value_parser>{{}, {}, "HH1 weight"sv}
	};
};

struct compress::shifts_parser {
	using value_t = shifts;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		return value_t{ {
			cx_parser.get<0>(),
			cx_parser.get<1>(),
			cx_parser.get<2>(),
			cx_parser.get<3>(),
			cx_parser.get<4>(),
			cx_parser.get<5>(),
			cx_parser.get<6>(),
			cx_parser.get<7>(),
			cx_parser.get<8>(),
			cx_parser.get<9>() 
		} };
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<uint>...[10]"sv;

	using default_representation_t = value_t;
};


template <>
struct parameter_context<compress::frame_dimensions> : public parameter_context_default {
	static constexpr immediate_parameters_description_t immediates{
		parameter_description<unsigned_integer_parser<>>{{}, {}, "DWT moving frame width"sv},
		parameter_description<unsigned_integer_parser<>>{{}, {}, "DWT moving frame height"sv}
	};
};

struct compress::frame_dimensions_parser {
	using value_t = frame_dimensions;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		return value_t{
			cx_parser.get<0>(), 
			cx_parser.get<1>()
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<frame_params>"sv;

	using default_representation_t = value_t;
};


template <>
struct parameter_context<compress::dwt> : public parameter_context_default {
	static constexpr compress::shifts default_shifts = { { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 } };
	static constexpr compress::frame_dimensions default_dims = { 0, 0 };

	static constexpr immediate_parameters_description_t immediates{
		parameter_description<enum_parser<compress::dwt_type>>{{}, compress::dwt_type::integer, "Discrete Wavelet Transform type to apply to image"sv}
	};

	static constexpr named_parameters_description_t named{
		parameter_description<compress::frame_dimensions_parser>{"--frame"sv, {default_dims}, "Concurrent DWT processing frame dimensions"sv},
		parameter_description<compress::shifts_parser>{"--shifts"sv, {default_shifts}, "Custom weight coefficients for integer DWT"sv},
	};
};

struct compress::dwt_parser {
	using value_t = dwt;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		std::optional<shifts> shifts_values;
		const dwt_type& type_value = cx_parser.get<0>();
		if (type_value == dwt_type::integer) {
			shifts_values = cx_parser.get<parser_t::name_to_index("--shifts"sv)>();
		}
		else {
			if (!cx_parser.if_default<parser_t::name_to_index("--shifts"sv)>()) {
				// TODO: parsing error, shifts are valid for int dwt only
			}
		}

		return value_t{
			type_value,
			cx_parser.get<parser_t::name_to_index("--frame"sv)>(),
			std::move(shifts_values)
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<dwt_params>"sv;
};


template <>
struct parameter_context<compress::segment::parameters_set> : public parameter_context_default {
	static constexpr named_parameters_description_t named{
		parameter_description<unsigned_integer_parser<>>{"--id"sv, {}, "First segment ID to apply setting to"sv}, 
		parameter_description<unsigned_integer_parser<>>{"--id-for"sv, {0}, "Number of consequtive segments to apply setting to"sv}, 
		parameter_description<unsigned_integer_parser<>>{"--id-until"sv, {0}, "Last segment ID to apply setting to"sv}, 
		parameter_description<unsigned_integer_parser<(1 << 20), 16>>{"--size"sv, {512}, "Segment size in blocks"sv}, 
		parameter_description<flag_parser>{"--heuristicDc"sv, {false}, "Use suboptimal code options for quantized DC coefficients"sv}, 
		parameter_description<flag_parser>{"--heuristicAc"sv, {false}, "Use suboptimal code options for AC coefficients bit depths description"sv}
	};
};

struct compress::segment_parser {
	using value_t = segment;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		// TODO: variable length arguments parsing needed here
		using parser_t = contextual_parser<segment::parameters_set>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		return value_t{
			{
				cx_parser.get<parser_t::name_to_index("--id"sv)>(),
				cx_parser.get<parser_t::name_to_index("--size"sv)>(),
				cx_parser.get<parser_t::name_to_index("--heuristicDc"sv)>(),
				cx_parser.get<parser_t::name_to_index("--heuristicAc"sv)>()
			}
		};
	}

	static consteval value_t make_default() {
		using parser_t = contextual_parser<segment::parameters_set>;

		constexpr size_t first_id = 0;
		return value_t{
			{
				first_id,
				parser_t::get_default<parser_t::name_to_index("--size"sv)>(),
				parser_t::get_default<parser_t::name_to_index("--heuristicDc"sv)>(),
				parser_t::get_default<parser_t::name_to_index("--heuristicAc"sv)>()
			}
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<segment_params>"sv;

	using default_representation_t = segment::parameters_set;
};


template <>
struct parameter_context<compress::stream::parameters_set> : public parameter_context_default {
	static constexpr named_parameters_description_t named{
		parameter_description<unsigned_integer_parser<>>{"--id"sv, {}, "First segment ID to apply setting to"sv},
		parameter_description<unsigned_integer_parser<>>{"--id-for"sv, {0}, "Number of consequtive segments to apply setting to"sv},
		parameter_description<unsigned_integer_parser<>>{"--id-until"sv, {0}, "Last segment ID to apply setting to"sv},
		parameter_description<unsigned_integer_parser<(1 << 27), 8>>{"--limit"sv, {1 << 27}, "Maximum size of segment in bytes"sv},
		parameter_description<unsigned_integer_parser<31>>{"--stop-bitplane"sv, {0}, "Bit plane index to stop encoding at"sv},
		parameter_description<unsigned_integer_parser<4>>{"--stop-stage"sv, {4}, "Bit plane encoding stage number to stop encoding at"sv},
		parameter_description<flag_parser>{"--stop-dc"sv, {false}, "Indicates to stop segment encoding after quantized DC coefficients are encoded"sv},
		parameter_description<flag_parser>{"--fill"sv, {false}, "Use fill value {0} to produce segment of size --limit"sv}
	};
};

struct compress::stream_parser {
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
				cx_parser.get<parser_t::name_to_index("--stop-dc"sv)>(),
				cx_parser.get<parser_t::name_to_index("--fill"sv)>()
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
				parser_t::get_default<parser_t::name_to_index("--stop-dc"sv)>(),
				parser_t::get_default<parser_t::name_to_index("--fill"sv)>()
			}
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<stream_params>"sv;

	using default_representation_t = stream::parameters_set;
};


template <>
struct parameter_context<compress::compress_command> : public parameter_context_default {
	static constexpr compress::segment default_segment = compress::segment_parser::make_default();
	static constexpr compress::stream default_stream = compress::stream_parser::make_default();

	// TODO: default values for segmentation and compression parameters
	static constexpr named_parameters_description_t named{
		parameter_description<compress::source_parser>{"--src"sv, {}, "Specifies input settings"sv},
		parameter_description<compress::destination_parser>{"--dst"sv, {}, "Specifies output settings"sv},
		parameter_description<compress::segment_parser>{"--segment"sv, {default_segment}, "Specifies segmentation settings"sv},
		parameter_description<compress::stream_parser>{"--stream"sv, {default_stream}, "Specifies output bit stream compression settings for segments"sv},
		parameter_description<compress::dwt_parser>{"--dwt"sv, {}, "Discrete Wavelet Transform type and parameters"sv},
		parameter_description<flag_parser>{"--signed"sv, {false}, "Indicates that input image has signed pixel values"sv},
		parameter_description<flag_parser>{"--transpose"sv, {false}, "Transpose input image before processing"sv}
	};
};

struct compress::compress_parser {
	using value_t = compress_command;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		using parser_t = contextual_parser<value_t>;
		parser_t cx_parser;
		cx_parser.parse(tokens);

		return value_t{
			cx_parser.get<parser_t::name_to_index("--src"sv)>(),
			cx_parser.get<parser_t::name_to_index("--dst"sv)>(),
			cx_parser.get<parser_t::name_to_index("--signed"sv)>(),
			cx_parser.get<parser_t::name_to_index("--transpose"sv)>(),
			cx_parser.get<parser_t::name_to_index("--dwt"sv)>(),
			cx_parser.get<parser_t::name_to_index("--stream"sv)>(), 
			cx_parser.get<parser_t::name_to_index("--segment"sv)>()
		};
	}

public:
	static constexpr std::string_view requirements = ""sv;
	static constexpr std::string_view placeholder = "<compress_params>"sv;
};

}
