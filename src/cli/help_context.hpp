#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include <cstddef>

#include "parameters/parameters_context.hpp"

// param help:
//	+ name
//	+ value placeholder
//	+ param description
//	+ parser requirement
// 
// immediate parameters miss param description?
// but should have it. therefore name section should be optional?
// e.g. --frame 1024 512 1
// or --frame description should mention value order?
// 
// --shifts is example where parent name description becomes messy
// 
// context help:
//	+ sequence of immediate placeholders
//	+ named placeholder, if has named parameters
//	+ variable length sequence placeholder, if has vla
//	+ list of named parameters, with corresponding param description; 
//		if mandatory specification, and default value description if optional
//	+ vla description section, if applicable
//	
// --help is meaningful in some nested context. After --help token is parsed, 
// parsing stops, unparsed tokens are skipped.
// build help description, allocate dynamically, pass to exception object and throw.
// handle user interface logic in the root frame.
// 
// differentiate between context help and param help? 
//	vector of strings divided by paragraphs?
//	variant with strong types?
//	base pointer with derived implementations? but interfaces are not the same
//	
// context help and param help are not exclusive! both needed when --help token is parsed 
// at position 0 parameter

struct help_context {
	struct description {
		bool if_positional;
		bool if_mandatory;
		bool if_vla;
		std::string_view name;
		std::string_view description;
		std::string_view placeholder;
		std::string_view default_value;
		std::string_view requirements;
	};

	// struct parameter_help {
	// 	std::string_view name;
	// 	std::string_view placeholder;
	// 	std::string_view description;
	// 	std::string_view requirements;
	// };

	struct context_help {
		std::string overview;
		std::vector<description> parameters_description;
	};

	struct global_help {
		std::array<description, 2> parameters_description;
	};

	std::optional<description> param_help;
	std::optional<context_help> cx_help;
	std::optional<global_help> global_cx_help;

	static global_help make_global_help() {
		using namespace cli::parameters;

		return global_help{{{
			{
				false, false, false, 
				global_context::help.name, 
				global_context::help.help_description, 
				std::string_view{},
				std::string_view{},
				std::string_view{}
			}, 
			{
				false, false, false,
				global_context::context_exit.name,
				global_context::context_exit.help_description,
				std::string_view{},
				std::string_view{},
				std::string_view{}
			}
		}}};
	}
};
