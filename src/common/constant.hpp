#pragma once

#include <bit>
#include <cstddef>

enum class dwt_type_t {
	idwt,
	fdwt
};

namespace constants {
	// TODO: there's a naming issue: constants classes have the same names as corresponding data
	// types; when both names are available in the scope hiding/shadowing is involved.
	//
	class block {
	public:
		static constexpr size_t items_per_block = 64;	// TODO: global constant as a property of a block struct
		static constexpr size_t block_size_mask = items_per_block - 1;
		static constexpr size_t block_size_shift = std::bit_width(std::bit_ceil(items_per_block) - 1); // approx for 
	};
	
	class gaggle {
	public:
		// TODO: rename to blocks_per_gaggle?
		static constexpr size_t items_per_gaggle = 16; // the value is assumed to be POT, see below
		static constexpr size_t gaggle_size_mask = items_per_gaggle - 1;
		static constexpr size_t gaggle_size_shift = std::bit_width(std::bit_ceil(items_per_gaggle) - 1); // approx for log
	};

	class subband {
	public:
		static constexpr size_t subbands_per_img = 10;
		static constexpr size_t max_scale_shift = 3;
		static constexpr size_t subbands_per_generation = 3;
		static constexpr size_t generation_num = 3;
		static constexpr size_t families_num = 3;
		static constexpr size_t subbands_per_family = 3;
	};

	class segment {
	public:
		static constexpr size_t max_blocks_per_segment = 2 << 20;
		static constexpr size_t min_blocks_per_segment = 16;
	};
}
