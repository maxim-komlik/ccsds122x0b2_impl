#include "io.hpp"

#include <atomic>
#include <string_view>
#include <limits>
#include <charconv>
#include <algorithm>

#include "io/file_sink.tpp"
#include "io/memory_sink.tpp"

namespace {
	size_t generate_sink_id() noexcept {
		static std::atomic<size_t> current_id = 0;
		// return current_id++; // equivalent to fetch_add; TODO: relax...
		return current_id.fetch_add(1, std::memory_order_relaxed);
	}
}

template <typename T>
std::unique_ptr<sink<T>> make_sink(const session_context& context, const data_descriptor& descriptor) {
	size_t sink_id = generate_sink_id();

	using namespace std::literals;

	switch (context.data_registry.get_segment_storage_type()) {
	case storage_type::file: {
		segment_file_descriptor& descriptor_value =
			io_data_registry::get_data<segment_selector<storage_type::file>>(descriptor);

		// sure we'd like u8 strings in the code below...
		// but C++20/23/26 does not provide overloads of std::to_chars for char8_t =(
		//

		constexpr std::string_view extension_suffix = ".ccsds"sv;
		constexpr std::string_view segment_description = "segment"sv;
		constexpr std::string_view channel_description = "channel"sv;
		constexpr std::string_view separator = "_"sv;
		constexpr size_t name_max_size =
			std::numeric_limits<size_t>::digits10 +		// {session id}
			separator.size() +							// _
			channel_description.size() +				// channel
			separator.size() +							// _
			std::numeric_limits<size_t>::digits10 +		// {channel index}
			separator.size() +							// _
			segment_description.size() +				// segment
			separator.size() +							// _
			std::numeric_limits<size_t>::digits10 +		// {segment id}
			separator.size() +							// _
			std::numeric_limits<size_t>::digits10 +		// {sink id}
			extension_suffix.size() +					// .ccsds
			sizeof(char);								// {terminator}

		std::array<char, name_max_size> filename_buffer{ char{} };
		// no errors expected during text processing
		auto conversion_result = std::to_chars(filename_buffer.data(), 
				filename_buffer.data() + filename_buffer.size(), context.id);
		if (conversion_result.ec != std::errc{}) {
			// C++23 nonreachable
		}

		auto pos = std::copy_n(separator.data(), separator.size(), conversion_result.ptr);
		pos = std::copy_n(channel_description.data(), channel_description.size(), pos);
		pos = std::copy_n(separator.data(), separator.size(), pos);

		conversion_result = std::to_chars(pos,
			filename_buffer.data() + filename_buffer.size(), descriptor_value.channel_id);
		if (conversion_result.ec != std::errc{}) {
			// C++23 nonreachable
		}

		pos = std::copy_n(separator.data(), separator.size(), conversion_result.ptr);
		pos = std::copy_n(segment_description.data(), segment_description.size(), pos);
		pos = std::copy_n(separator.data(), separator.size(), pos);

		conversion_result = std::to_chars(pos, 
				filename_buffer.data() + filename_buffer.size(), descriptor_value.segment_id);
		if (conversion_result.ec != std::errc{}) {
			// C++23 nonreachable
		}

		pos = std::copy_n(separator.data(), separator.size(), conversion_result.ptr);
		
		conversion_result = std::to_chars(pos, 
				filename_buffer.data() + filename_buffer.size(), sink_id);
		if (conversion_result.ec != std::errc{}) {
			// C++23 nonreachable
		}

		pos = std::copy_n(extension_suffix.data(), extension_suffix.size(), conversion_result.ptr);

		std::string_view path_view(filename_buffer.data(), pos);

		descriptor_value.path = std::filesystem::path(path_view);
		return std::make_unique<file_sink<T>>(descriptor_value);
	}
	case storage_type::memory: {
		segment_memory_descriptor& descriptor_value =
			io_data_registry::get_data<segment_selector<storage_type::memory>>(descriptor);
		return std::make_unique<memory_sink<T>>(descriptor_value);
	}
	default: {

	}
	}
	return nullptr; // TODO: not impelemented error handling

}

template <typename T>
std::unique_ptr<source<T>> make_source(const session_context& context, const data_descriptor& descriptor) {
	size_t source_id = generate_sink_id();

	switch (context.data_registry.get_segment_storage_type()) {
	case storage_type::file: {
		segment_file_descriptor& descriptor_value =
			io_data_registry::get_data<segment_selector<storage_type::file>>(descriptor);
		return std::make_unique<file_source<T>>(descriptor_value);
	}
	case storage_type::memory: {
		segment_memory_descriptor& descriptor_value =
			io_data_registry::get_data<segment_selector<storage_type::memory>>(descriptor);
		return std::make_unique<memory_source<T>>(descriptor_value);
	}
	default: {
		break;
	}
	}
	return nullptr; // TODO: not impelemented error handling
}


// explicit instantiation section

template std::unique_ptr<sink<int8_t>> make_sink<int8_t>(const session_context& constext, const data_descriptor& descriptor);
template std::unique_ptr<sink<int16_t>> make_sink<int16_t>(const session_context& constext, const data_descriptor& descriptor);
template std::unique_ptr<sink<int32_t>> make_sink<int32_t>(const session_context& constext, const data_descriptor& descriptor);
template std::unique_ptr<sink<int64_t>> make_sink<int64_t>(const session_context& constext, const data_descriptor& descriptor);

template std::unique_ptr<sink<uint8_t>> make_sink<uint8_t>(const session_context& constext, const data_descriptor& descriptor);
template std::unique_ptr<sink<uint16_t>> make_sink<uint16_t>(const session_context& constext, const data_descriptor& descriptor);
template std::unique_ptr<sink<uint32_t>> make_sink<uint32_t>(const session_context& constext, const data_descriptor& descriptor);
template std::unique_ptr<sink<uint64_t>> make_sink<uint64_t>(const session_context& constext, const data_descriptor& descriptor);

template std::unique_ptr<source<int8_t>> make_source<int8_t>(const session_context& context, const data_descriptor& descriptor);
template std::unique_ptr<source<int16_t>> make_source<int16_t>(const session_context& context, const data_descriptor& descriptor);
template std::unique_ptr<source<int32_t>> make_source<int32_t>(const session_context& context, const data_descriptor& descriptor);
template std::unique_ptr<source<int64_t>> make_source<int64_t>(const session_context& context, const data_descriptor& descriptor);

template std::unique_ptr<source<uint8_t>> make_source<uint8_t>(const session_context& context, const data_descriptor& descriptor);
template std::unique_ptr<source<uint16_t>> make_source<uint16_t>(const session_context& context, const data_descriptor& descriptor);
template std::unique_ptr<source<uint32_t>> make_source<uint32_t>(const session_context& context, const data_descriptor& descriptor);
template std::unique_ptr<source<uint64_t>> make_source<uint64_t>(const session_context& context, const data_descriptor& descriptor);
