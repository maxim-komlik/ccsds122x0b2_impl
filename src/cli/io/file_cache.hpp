#pragma once

#include "bmp/header_protocol.hpp"

#include <unordered_map>
#include <any>
#include <variant>
#include <filesystem>
#include <functional>

#include "io/file_sink.tpp"
#include "io/io_data_registry.hpp"


struct external_file_descriptor : public file_descriptor {
	external_file_descriptor(const std::filesystem::path& path) : file_descriptor(path) {}
	external_file_descriptor(std::filesystem::path&& path) : file_descriptor(std::move(path)) {}
};

namespace cli::io {

struct plain_protocol {};

struct file_cache_key {
	std::filesystem::path path;
	std::filesystem::file_time_type last_written;

	friend bool operator==(const file_cache_key& lhs, const file_cache_key& rhs) = default;
};

struct file_cache_value {
	file_source<size_t> src;
	std::variant<
		plain_protocol,
		bmp_header_protocol> file_protocol;
	std::any content_data;

public:
	file_cache_value(const file_descriptor& descriptor) : src(descriptor) {}
};

}


template <>
struct std::hash<cli::io::file_cache_key> {
	size_t operator()(const cli::io::file_cache_key& key) const noexcept;
};

namespace cli::io {

class file_cache {
private:
	std::unordered_map<file_cache_key, external_file_descriptor> descriptor_cache;
	std::unordered_map<const file_descriptor*, file_cache_value> cache_content;

public:

	const file_descriptor& get_descriptor(const std::filesystem::path& path);
	file_cache_value& get_data(const file_descriptor& descriptor);

	// TODO: descriptor free interface
};


extern file_cache file_cache_instance;

}
