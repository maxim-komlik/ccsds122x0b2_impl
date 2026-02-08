#pragma once

#include <cstdint>

#include "common/constant.hpp"
#include "utils.hpp"

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

enum class storage_type {
	file,
	memory, 
	network
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

	bool valid = true;
	valid &= ((bits_per_word & bindex_mask) == 0);
	valid &= (bits_per_word <= 64);
	valid &= (bits_per_word > 0);

	if (!valid) {
		throw "Invalid word length!";	// TODO: throw proper type
	}

	//	0001 000		00 0
	//	0010 000		01 0
	//	0011 000		10 0
	//	0100 000		11 0
	//	0101 000		00 1
	//	0110 000		01 1
	//	0111 000		10 1
	//	1000 000		11 1
	//

	bits_per_word >>= 3;
	--bits_per_word;
	return (codeword_length)(((bits_per_word << 1) & bindex_mask) ^ (bits_per_word >> 2));
}

constexpr size_t parse_codeword_length_value(codeword_length value) {
	bool valid = true;
	valid &= to_underlying(value) < 0b1000;
	if (!valid) {
		// TODO: error handling
	}

	return (((to_underlying(value) >> 1) + 1) << 3) + ((to_underlying(value) & 1) << 5);
}
