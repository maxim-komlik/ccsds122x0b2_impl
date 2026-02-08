#pragma once

#include <bit>

#include "common/constant.hpp"

// TODO: move dbg parameters to dedicated files
namespace dbg {
	namespace segment {
		static constexpr uint32_t mask_reverse_DC = 0x01;
		static constexpr uint32_t mask_reverse_bdepthAC = 0x02;

		namespace encoder {
			static constexpr uint32_t disabled_stages = 0x03;

			constexpr bool if_enabled(uint32_t mask) {
				return (disabled_stages & mask) == 0;
			}

			constexpr bool if_disabled(uint32_t mask) {
				return !if_enabled(mask);
			}
		};

		namespace decoder {
			static constexpr uint32_t disabled_stages = 0x00;

			constexpr bool if_enabled(uint32_t mask) {
				return (disabled_stages & mask) == 0;
			}

			constexpr bool if_disabled(uint32_t mask) {
				return !if_enabled(mask);
			}
		};
	};

	namespace dwt {
		static constexpr uint32_t mask_reverse_data = 0x01;

		namespace encoder {
			static constexpr uint32_t disabled_stages = 0x01;

			constexpr bool if_enabled(uint32_t mask) {
				return (disabled_stages & mask) == 0;
			}

			constexpr bool if_disabled(uint32_t mask) {
				return !if_enabled(mask);
			}
		};

		namespace decoder {
			static constexpr uint32_t disabled_stages = 0x01;

			constexpr bool if_enabled(uint32_t mask) {
				return (disabled_stages & mask) == 0;
			}

			constexpr bool if_disabled(uint32_t mask) {
				return !if_enabled(mask);
			}
		};
	}
};

namespace constants {
	class img {
	public:
		static constexpr size_t granularity = 16;
		static constexpr size_t min_width = 17;
		static constexpr size_t max_width = 1 << 20;
		static constexpr size_t min_height = 17;
	};

	class dwt {
	public:
		static constexpr size_t level_num = 3;

	protected:
		static constexpr size_t horizontal_padding_length = 8;
		static constexpr size_t horizontal_padding_mask = horizontal_padding_length - 1;
		static constexpr size_t horizontal_padding_shift = std::bit_width(std::bit_ceil(horizontal_padding_length) - 1); // approx for 

		static constexpr size_t vertical_padding_length = 8;
		static constexpr size_t vertical_padding_mask = vertical_padding_length - 1;
		static constexpr size_t vertical_padding_shift = std::bit_width(std::bit_ceil(vertical_padding_length) - 1); // approx for 

		static_assert(subband::subbands_per_img == level_num * 3 + 1);

		static constexpr ptrdiff_t LL3_index = 8;

		// largest possible overlap combination for lowpass level 3 LL3:
		//	3 levels of 2d lowpass, coefficient dependency range +-4; 
		//	double size buffers (and corresponding overlap region size) for every lower level:
		//		4		LL2 dependency overlap (direct source for level 3 transform)
		//		*=2		LL2 -> LL1 level resolution downstep (or just +3 highpass dependency overlap?)
		//		+=4		LL1 dependency overlap (direct source for level 2 transform)
		//		*=2		LL1 -> source level resolution downstep
		//		== 24
		//		+=4		source dependency overlap? (direct source for level 1 transform) ? // TODO: 
		//	overlap region size is the same for vertical and horizontal data.
		// 
		static constexpr size_t overlap_img_range = 28; // 24;
		static constexpr size_t overlap_crossrow_buffer_width = 16;

		static constexpr size_t lowpass_dependency_radius = 4;
		static constexpr size_t highpass_dependency_radius = 3;

		static constexpr size_t fwd_buffers_offset_value = 32;	// dwtcore.fwd() reads up to 62 items oob
		static constexpr size_t bwd_buffers_offset_value = 32;	// dwtcore.bwd() writes up to 48 items oob
	};
}
