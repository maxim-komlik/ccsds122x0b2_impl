#pragma once

#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include <variant>
#include <cstddef>

#include "parameters_context.hpp"
#include "meta.hpp"

namespace cli::parameters::compress {

namespace generate {

struct image_dimensions {
	size_t width;
	size_t height;
	size_t depth;
};

struct generator {
	image_dimensions dims;
	size_t bdepth;
	bool pixel_signed;
	ptrdiff_t generator_seed;
};

}

enum class src_type {
	generate
};

struct source {
	src_type type;
	std::variant<generate::generator> parameters;
};

enum class dst_type {
	file,
	memory
};

enum class protocol_type {
	standard,
	file,
	memory
};


struct file_sink_params {};
struct memory_sink_params {};


struct destination {
	dst_type type;
	protocol_type protocol;
	std::variant<file_sink_params, memory_sink_params> parameters;
};

enum class dwt_type {
	integer,
	fp
};

struct shifts {
	std::array<size_t, 10> values;
};

struct frame_dimensions {
	size_t width;
	size_t height;
};

struct dwt {
	dwt_type type;
	frame_dimensions frame;
	std::optional<shifts> shifts;
};

struct segment {
	struct parameters_set {
		size_t id;
		size_t size;
		bool heuristic_DC;
		bool heuristic_AC_bdepth;
	};

	parameters_set first;
	// default constructor for std::vector is noexcept, therefore considered as non-allocating.
	// thus, constexpr instance is possible and valid (but see notes for details)
	std::vector<parameters_set> subsequent;

	constexpr operator parameters_set() const {
		return this->first;
	}
};

struct stream {
	struct parameters_set {
		size_t id;
		size_t byte_limit;
		size_t bplane_stop;
		size_t stage_stop;
		bool DC_stop;
		bool fill;
	};

	parameters_set first;
	// default constructor for std::vector is noexcept, therefore considered as non-allocating.
	// thus, constexpr instance is possible and valid (but see notes for details)
	std::vector<parameters_set> subsequent;

	constexpr operator parameters_set() const {
		return this->first;
	}
};

struct compress_command {
	source src_params;
	destination dst_params;
	bool img_signed;
	bool img_transpose;
	dwt dwt_params;
	stream stream_params;
	segment segment_params;
};

}

namespace meta {
	using namespace std::literals;

	template <>
	struct enumerators_mapping<cli::parameters::compress::src_type> {
		using src_type = cli::parameters::compress::src_type;

		constexpr static enumerator_mapping_description_t description{
			enumerator_mapping_item_t{ src_type::generate, "generate"sv }
		};
	};

	template <>
	struct enumerators_mapping<cli::parameters::compress::dst_type> {
		using dst_type = cli::parameters::compress::dst_type;

		constexpr static enumerator_mapping_description_t description{
			enumerator_mapping_item_t{ dst_type::file, "file"sv },
			enumerator_mapping_item_t{ dst_type::memory, "memory"sv }
		};
	};

	template <>
	struct enumerators_mapping<cli::parameters::compress::protocol_type> {
		using protocol_type = cli::parameters::compress::protocol_type;

		constexpr static enumerator_mapping_description_t description{
			enumerator_mapping_item_t{ protocol_type::standard, "standard"sv },
			enumerator_mapping_item_t{ protocol_type::file, "file"sv },
			enumerator_mapping_item_t{ protocol_type::memory, "memory"sv }
		};
	};

	template <>
	struct enumerators_mapping<cli::parameters::compress::dwt_type> {
		using dwt_type = cli::parameters::compress::dwt_type;

		constexpr static enumerator_mapping_description_t description{
			enumerator_mapping_item_t{ dwt_type::integer, "integer"sv },
			enumerator_mapping_item_t{ dwt_type::fp, "fp"sv }
		};
	};


	template <>
	struct member_names_trait<cli::parameters::compress::shifts> {
		static constexpr member_names_description_t names = {
			"values"sv
		};
	};

	template <>
	struct member_names_trait<cli::parameters::compress::frame_dimensions> {
		static constexpr member_names_description_t names = {
			"width"sv, 
			"height"sv
		};
	};

	template <>
	struct member_names_trait<cli::parameters::compress::dwt> {
		static constexpr member_names_description_t names = {
			"type"sv,
			"frame"sv,
			"shifts"sv
		};
	};

	template <>
	struct member_names_trait<cli::parameters::compress::segment::parameters_set> {
		static constexpr member_names_description_t names = {
			"id"sv,
			"size"sv,
			"heuristic_DC"sv,
			"heuristic_AC_bdepth"sv
		};
	};

	template <>
	struct member_names_trait<cli::parameters::compress::stream::parameters_set> {
		static constexpr member_names_description_t names = {
			"id"sv,
			"byte_limit"sv,
			"bplane_stop"sv,
			"stage_stop"sv,
			"DC_stop"sv,
			"fill"sv
		};
	};
}
