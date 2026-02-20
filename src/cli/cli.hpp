#pragma once

#include <string_view>

namespace cli {

	using namespace std::literals;

	// namespace declarations with following extensions break msvc for some reason
	// 
	// namespace parameters {}
	// 
	// namespace parsers {}
	// 
	// namespace validate {
	// 	using namespace parameters;
	// }
	// 
	// namespace command {
	// 	using namespace parameters;
	// }
	
}
