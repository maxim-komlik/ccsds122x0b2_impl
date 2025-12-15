#pragma once

#include "core_types.hpp"
// #include "dwt/img_meta.h"
#include "constant.hpp"

// struct img_context {
// 	// static parameters, remain unchanged throghout the processing
// 	bool signed_pixel;
// 	size_t pixel_bdepth;
// 
// 	// mutable state params
// 	bool transpose;
// 	size_t rows_pad_count;	// header part 1b
// };
// 
// struct img_settings {
// 	// static parameters, remain unchanged throghout the processing
// 	bool signed_pixel;
// 	size_t pixel_bdepth;
// 	img_meta meta;
// 
// 	// other params are rather property/belong to state of a session
// };

struct session_settings {
	dwt_type_t dwt_type;

	// img_meta meta; // but image width only is strictly needed
	size_t img_width;

	size_t pixel_bdepth;
	bool signed_pixel;

	bool transpose;
	size_t rows_pad_count;	// header part 1b

	size_t codeword_size;

	bool custom_shifts;
	shifts_t shifts;
};

struct segment_settings {
	size_t size;	// but actually stored in segment itself
	bool heuristic_quant_DC;
	bool heiristic_bdepth_AC;
};

struct compression_settings {
	size_t seg_byte_limit;
	bool use_fill;

	bool early_termination;
	bool DC_stop;
	size_t bplane_stop;
	size_t stage_stop;
};
