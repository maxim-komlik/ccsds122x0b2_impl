#pragma once

#include "common/constant.h"

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
};

namespace constants {
	class img {
	public:
		static constexpr size_t granularity = 16;
		static constexpr size_t min_width = 17;
		static constexpr size_t height_limit = 0x01 << 11;
		static constexpr size_t length_limit = 0x01 << 28;
	};
}
