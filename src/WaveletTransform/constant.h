#pragma once

#include "common/constant.h"

namespace constants {
	class img {
	public:
		constexpr size_t granularity = 16;
		constexpr size_t min_width = 17;
		constexpr size_t height_limit = 0x01 << 11;
		constexpr size_t length_limit = 0x01 << 28;
	};
}
