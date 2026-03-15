#include "file_cache.hpp"

namespace cli::io {

const file_descriptor& file_cache::get_descriptor(const std::filesystem::path& path) {
	std::filesystem::file_time_type last_written = std::filesystem::last_write_time(path);

	file_cache_key key{ path, last_written };

	if (!descriptor_cache.contains(key)) {
		auto [it, inserted] = descriptor_cache.insert({ key, {path} });
		cache_content.emplace(&(it->second), it->second);
	}

	return descriptor_cache.at(key);
}


file_cache_value& file_cache::get_data(const file_descriptor& descriptor) {
	return this->cache_content.at(&descriptor);
}


file_cache file_cache_instance;

}


size_t std::hash<cli::io::file_cache_key>::operator()(const cli::io::file_cache_key& key) const noexcept {
	auto combine = [](size_t lhs, size_t rhs) noexcept -> size_t {
		constexpr size_t salt = std::invoke([]() constexpr {
			// they say below is hex representation of floating point representation of pi approximation
			// 
			// size_t salt = 0xc90fdff22168c234c4c66280dc1cd1u; // this causes ill-formed
			constexpr std::array salt_bytes = { 
				std::byte{0xc9u},
				std::byte{0x0fu}, 
				std::byte{0xdfu}, 
				std::byte{0xf2u}, 
				std::byte{0x21u}, 
				std::byte{0x68u}, 
				std::byte{0xc2u}, 
				std::byte{0x34u}, 
				std::byte{0xc4u}, 
				std::byte{0xc6u}, 
				std::byte{0x62u}, 
				std::byte{0x80u}, 
				std::byte{0xdcu}, 
				std::byte{0x1cu}, 
				std::byte{0xd1u}
			};

			size_t result{};
			for (ptrdiff_t i = 0; i < sizeof(size_t); ++i) {
				result |= static_cast<size_t>(salt_bytes[i]) << ((sizeof(size_t) - 1 - i) << 3);
			}

			return result;
		});

		return (lhs + (rhs << 3) + (rhs >> 2)) ^ salt;
	};

	// hash support for time points is added in C++26 only
	return combine(std::hash<decltype(key.path)>{}(key.path), static_cast<size_t>(key.last_written.time_since_epoch().count()));
}
