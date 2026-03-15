#include "image.hpp"

#include <string>
#include <string_view>
#include <array>
#include <functional>

#include "generate/params.hpp"
#include "bmp/params.hpp"

namespace cli::io {

image_load_parameters get_image_description(const parameters::compress::source& parameters) {
	namespace params = parameters::compress;

	auto make_meta = [](decltype(parameters) params) -> image_description {
		switch (params.type) {
		case params::src_type::generate: {
			return generate::get_description(std::get<params::generate::generator>(params.parameters));
			break;
		}
		case params::src_type::file: {
			auto& file_params = std::get<params::image_file>(params.parameters);
			std::u8string extension = file_params.path.extension().u8string();

			constexpr std::array known_extensions = std::invoke([]() constexpr {
					using namespace std::literals;

					using handler_t = image_description(*)(const parameters::compress::image_file&);

					std::pair<std::u8string_view, handler_t> values[] = {
						{u8".bmp"sv, bmp::get_description}
					};
					return std::to_array(values);
				});

			auto it = std::find_if(known_extensions.cbegin(), known_extensions.cend(),
				[&extension](const auto& item) -> bool { return item.first == extension; });

			bool valid = true;
			valid &= (it != known_extensions.cend());
			if (!valid) {
				// TODO: error handling
			}

			return std::invoke(it->second, file_params);
		}
		default: {

		}
		}

		// TODO: C++23 std::unreachable?
	};

	return image_load_parameters { parameters, make_meta(parameters)};
}

}
