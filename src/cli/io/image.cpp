#include "image.hpp"

#include <string>
#include <string_view>
#include <array>
#include <filesystem>
#include <utility>
#include <cstddef>

#include "dwt/bitmap.tpp"
#include "dwt/bitmap_types.hpp"

#include "file_cache.hpp"
#include "generate/params.hpp"
#include "generate/load.tpp"
#include "bmp/params.hpp"
#include "bmp/load.tpp"
#include "bmp/store.tpp"


namespace cli::io {

namespace {

	template <typename T>
	struct image_store_handler {
		void operator()(io_data_registry&& registry, const io::image_store_parameters& export_specs);
	};

	template <typename T>
	struct image_load_handler {
		void operator()(io_data_registry& registry, const io::image_load_parameters& import_spec,
			std::vector<std::reference_wrapper<const data_descriptor>>& descriptors);
	};

	template <typename T>
	bitmap<T> load_image_channel(const image_load_parameters& parameters, size_t channel_index);

}

image_load_parameters get_image_description(const parameters::compress::source& parameters) {
	namespace params = parameters::compress;

	auto make_meta = [](decltype(parameters) params) -> img_meta {
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

					using handler_t = img_meta(*)(const parameters::compress::image_file&);

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

std::vector<std::reference_wrapper<const data_descriptor>> import_image(
		io_data_registry& registry, const io::image_load_parameters& import_specs) {
	std::vector<std::reference_wrapper<const data_descriptor>> result;

	integral_bit_depth_dispatch<image_load_handler>::apply(
		import_specs.meta.bdepth_static, import_specs.meta.if_signed,
		registry, import_specs, result);

	return result;
}

void export_image(io_data_registry&& registry, const io::image_store_parameters& export_specs) {
	integral_bit_depth_dispatch<image_store_handler>::apply(
		export_specs.meta.bdepth_static, export_specs.meta.if_signed,
		std::move(registry), export_specs);
}


namespace {

	template <typename T>
	void image_store_handler<T>::operator()(
			io_data_registry&& registry, const io::image_store_parameters& export_specs) {
		auto handles = std::move(registry).export_data();

		bool valid = true;
		valid &= !handles.empty();

		if (!valid) {
			// TODO: error handling
		}

		std::vector<bitmap<T>> channels(handles.size());

		// that must have been a case for std::transform, but it requires that input argument is not modified
		for (auto&& item : handles) {
			auto& image_data = io_data_registry::get_data<image_selector<T>>(item);

			valid &= image_data.channel_id < channels.size();
			if (!valid) {
				// TODO: error handling, throw
			}

			channels[image_data.channel_id] = std::move(image_data.image);
		}

		namespace params = parameters::restore;

		switch (export_specs.cli_parameters.type) {
		case params::dst_type::file: {
			const auto& file_params = std::get<params::image_file>(export_specs.cli_parameters.parameters);
			std::u8string extension = file_params.path.extension().u8string();

			constexpr std::array known_extensions = std::invoke([]() constexpr {
					using namespace std::literals;

					using handler_t = void (*)(const std::vector<bitmap<T>>&, const std::filesystem::path&);

					std::pair<std::u8string_view, handler_t> values[] = {
						{u8".bmp"sv, bmp::store_image<T>}
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

			return std::invoke(it->second, channels, file_params.path);
		}
		default: {
			// no export needed?
		}
		}
	}

	template <typename T>
	void image_load_handler<T>::operator()(
			io_data_registry& registry, const io::image_load_parameters& import_spec,
			std::vector<std::reference_wrapper<const data_descriptor>>& descriptors) {
		bool valid = true;
		valid &= descriptors.empty();

		if (!valid) {
			// TODO: error handling
		}

		std::vector<img_meta> channel_stats;
		std::vector<size_t> channel_bdepths;

		channel_stats.reserve(import_spec.meta.depth);
		channel_bdepths.reserve(import_spec.meta.depth);
		descriptors.reserve(import_spec.meta.depth);

		for (ptrdiff_t i = 0; i < import_spec.meta.depth; ++i) {
			auto channel_data = load_image_channel<T>(import_spec, i);
			channel_stats.push_back(channel_data.get_meta());
			channel_bdepths.push_back(bitmap_dynamic_bit_depth(channel_data));
			// TODO: it appears bdepth should be property of channel, not session

			const data_descriptor& descriptor = registry.put_input(
				image_memory_descriptor(std::move(channel_data), i));
			descriptors.push_back(descriptor);
		}

		auto it_stats = std::find_if_not(channel_stats.cbegin(), channel_stats.cend(),
			[&target = import_spec.meta](const img_meta& item) -> bool {
				return (item.width == target.width) & (item.height == target.height);
			});

		auto it_bdepth = std::find_if_not(channel_bdepths.cbegin(), channel_bdepths.cend(),
			[&target = import_spec.meta](size_t item) -> bool {
				return item <= target.bdepth_static;
			});

		valid &= !channel_stats.empty();
		valid &= (it_stats == channel_stats.cend());
		valid &= (it_bdepth == channel_bdepths.cend());
		if (!valid) {
			// TODO: throw, invalid image
		}
	}
	
	template <typename T>
	bitmap<T> load_image_channel(const image_load_parameters& parameters, size_t channel_index) {
		namespace params = parameters::compress;

		size_t image_row_offset = std::invoke([&]() -> size_t {
				// TODO: review
				if (parameters.meta.width < 64) {
					return 32;
				} else {
					return 16;
				}
			});

		switch (parameters.cli_parameters.type) {
		case params::src_type::generate: {
			const auto& gen_params = std::get<params::generate::generator>(parameters.cli_parameters.parameters);

			bool valid = true;
			valid &= channel_index < gen_params.dims.depth;
			valid &= gen_params.bdepth <= std::numeric_limits<T>::digits;
			valid &= gen_params.pixel_signed != std::is_signed_v<T>;
			if (!valid) {
				// TODO: error handling, throw
			}

			return generate::load_channel<T>(gen_params.dims.width, gen_params.dims.height,
				image_row_offset, gen_params.bdepth, gen_params.generator_seed);
			break;
		}
		case params::src_type::file: {
			const auto& file_params = std::get<params::image_file>(parameters.cli_parameters.parameters);
			std::u8string extension = file_params.path.extension().u8string();

			constexpr std::array known_extensions = std::invoke([]() constexpr {
					using namespace std::literals;

					using handler_t = bitmap<T>(*)(const file_descriptor&, ptrdiff_t, size_t);

					std::pair<std::u8string_view, handler_t> values[] = {
						{u8".bmp"sv, bmp::load_channel<T>}
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

			const file_descriptor& descriptor = file_cache_instance.get_descriptor(file_params.path);

			return std::invoke(it->second, descriptor, channel_index, image_row_offset);
		}
		default: {
			// TODO: error handling?
		}
		}

	}

}

}
