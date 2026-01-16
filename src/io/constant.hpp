#pragma once

#include <cstdint>

#include "common/constant.hpp"

// TODO: move dbg parameters to dedicated files
namespace dbg {
	namespace protocol {
		static constexpr uint32_t mask_forward_compatibility = 0x01;

		static constexpr uint32_t disabled_stages = 0x01;

		constexpr bool if_enabled(uint32_t mask) {
			return (disabled_stages & mask) == 0;
		}

		constexpr bool if_disabled(uint32_t mask) {
			return !if_enabled(mask);
		}
	};
};

enum class bpe_stage_index_t : uint8_t {
	stage_1 = 0b00, 
	stage_2 = 0b01, 
	stage_3 = 0b10, 
	stage_4 = 0b11
};

// tools for CodeWordLength field of HeaderPart_4 struct
enum class codeword_length : uint8_t {
	w8bit = 0b000, 
	w16bit = 0b010, 
	w24bit = 0b100, 
	w32bit = 0b110, 
	w40bit = 0b001, 
	w48bit = 0b011, 
	w56bit = 0b101, 
	w64bit = 0b111
};

// constexpr codeword_length getWordLengthValue(size_t bits_per_word) {
constexpr codeword_length get_codeword_length_value(size_t bits_per_word) {
	constexpr size_t bindex_mask = ~((-1) << 3);
	if ((bits_per_word & bindex_mask) || (bits_per_word > 64)) {
		throw "Invalid word length!";	// TODO: throw proper type
	}

	size_t &&bitNumShifted = bits_per_word >> 2; // most significant bit is moved to lowest position
	return (codeword_length)((bitNumShifted & bindex_mask) ^ ((bitNumShifted >> 3) & 0b01));
}
