#pragma once

#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include <variant>
#include <filesystem>
#include <cstddef>

#include "parameters_context.hpp"
#include "utility.hpp"

namespace cli::parameters::restore {

enum class src_type {
	file
};

enum class segment_protocol_type {
	detect,
	standard,
	file,
	memory
};

enum class dst_type {
	memory
};

enum class image_protocol_type {
	raw
};


struct file_sink_params {
	std::filesystem::path path;
};
struct memory_sink_params {};


struct source {
	src_type type;
	segment_protocol_type protocol;
	std::variant<file_sink_params, memory_sink_params> parameters;
};

struct destination {
	dst_type type;
	image_protocol_type protocol;
};

struct stream {
	struct parameters_set {
		size_t id;
		size_t byte_limit;
		size_t bplane_stop;
		size_t stage_stop;
		bool DC_stop;
	};

	parameters_set first;
	// default constructor for std::vector is noexcept, therefore considered as non-allocating.
	// thus, constexpr instance is possible and valid (but see notes for details)
	std::vector<parameters_set> subsequent;

	constexpr operator parameters_set() const {
		return this->first;
	}
};

struct restore_command {
	source src_params;
	destination dst_params;
	stream stream_params;
};

}

namespace meta {
	using namespace std::literals;

	template <>
	struct enumerators_mapping<cli::parameters::restore::src_type> {
		using src_type = cli::parameters::restore::src_type;

		constexpr static enumerator_mapping_description_t description{
			enumerator_mapping_item_t{ src_type::file, "file"sv }
		};
	};

	template <>
	struct enumerators_mapping<cli::parameters::restore::dst_type> {
		using dst_type = cli::parameters::restore::dst_type;

		constexpr static enumerator_mapping_description_t description{
			enumerator_mapping_item_t{ dst_type::memory, "memory"sv }
		};
	};

	template <>
	struct enumerators_mapping<cli::parameters::restore::segment_protocol_type> {
		using protocol_type = cli::parameters::restore::segment_protocol_type;

		constexpr static enumerator_mapping_description_t description{
			enumerator_mapping_item_t{ protocol_type::detect, "detect"sv },
			enumerator_mapping_item_t{ protocol_type::standard, "standard"sv },
			enumerator_mapping_item_t{ protocol_type::file, "file"sv },
			enumerator_mapping_item_t{ protocol_type::memory, "memory"sv }
		};
	};

	template <>
	struct enumerators_mapping<cli::parameters::restore::image_protocol_type> {
		using protocol_type = cli::parameters::restore::image_protocol_type;

		constexpr static enumerator_mapping_description_t description{
			enumerator_mapping_item_t{ protocol_type::raw, "raw"sv }
		};
	};

	template <>
	struct member_names_trait<cli::parameters::restore::destination> {
		static constexpr member_names_description_t names = {
			"type"sv, 
			"protocol"sv
		};
	};

	template <>
	struct member_names_trait<cli::parameters::restore::stream::parameters_set> {
		static constexpr member_names_description_t names = {
			"id"sv,
			"byte_limit"sv,
			"bplane_stop"sv,
			"stage_stop"sv,
			"DC_stop"sv
		};
	};
}
