#pragma once

#include <bit>

namespace constants {
	class block {
	public:
		static constexpr size_t items_per_block = 64;	// TODO: global constant as a property of a block struct
		static constexpr size_t block_size_mask = items_per_block - 1;
		static constexpr size_t block_size_shift = std::bit_width(std::bit_ceil(items_per_block) - 1); // approx for 
	};
	
	class gaggle {
	public:
		static constexpr size_t items_per_gaggle = 16; // the value is assumed to be POT, see below
		static constexpr size_t gaggle_size_mask = items_per_gaggle - 1;
		static constexpr size_t gaggle_size_shift = std::bit_width(std::bit_ceil(items_per_gaggle) - 1); // approx for log
	};
	
	class dwt {
	public:
		constexpr static size_t subband_num = 10;
	};
	
	class scale {
	public:
		static constexpr size_t max_subband_shift = 3;
	};
}
