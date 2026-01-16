#include "io.hpp"

#include <atomic>
#include <string_view>
#include <limits>
#include <charconv>
#include <algorithm>

#include "io/file_sink.hpp"
#include "io/memory_sink.hpp"

namespace {
	size_t generate_sink_id() noexcept {
		static std::atomic<size_t> current_id = 0;
		// return current_id++; // equivalent to fetch_add; TODO: relax...
		return current_id.fetch_add(1, std::memory_order_relaxed);
	}
}

template <typename T>
std::unique_ptr<sink<T>> make_sink(const session_context& context, size_t segment_id, sink_type type, size_t channel_index) {
	size_t sink_id = generate_sink_id();

	using namespace std::literals;

	switch (type) {
	case sink_type::file: {
		// sure we'd like u8 strings in the code below...
		// but C++20/23/26 does not provide overloads of std::to_chars for char8_t =(
		//

		constexpr std::string_view extension_suffix = ".ccsds"sv;
		constexpr std::string_view segment_description = "segment"sv;
		constexpr std::string_view separator = "_"sv;
		constexpr size_t name_max_size =
			std::numeric_limits<size_t>::digits10 +		// {session id}
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

		conversion_result = std::to_chars(pos,
			filename_buffer.data() + filename_buffer.size(), channel_index);
		if (conversion_result.ec != std::errc{}) {
			// C++23 nonreachable
		}

		pos = std::copy_n(separator.data(), separator.size(), conversion_result.ptr);
		pos = std::copy_n(segment_description.data(), segment_description.size(), pos);
		pos = std::copy_n(separator.data(), separator.size(), pos);

		conversion_result = std::to_chars(pos, 
				filename_buffer.data() + filename_buffer.size(), segment_id);
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
		return std::make_unique<file_sink<T>>(std::filesystem::path(path_view));
	}
	case sink_type::memory: {
		return std::make_unique<memory_sink<T>>();
	}
	default: {

	}
	}
	return nullptr; // TODO: not impelemented error handling
}

template std::unique_ptr<sink<int8_t>> make_sink<int8_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
template std::unique_ptr<sink<int16_t>> make_sink<int16_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
template std::unique_ptr<sink<int32_t>> make_sink<int32_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
template std::unique_ptr<sink<int64_t>> make_sink<int64_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);

template std::unique_ptr<sink<uint8_t>> make_sink<uint8_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
template std::unique_ptr<sink<uint16_t>> make_sink<uint16_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
template std::unique_ptr<sink<uint32_t>> make_sink<uint32_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
template std::unique_ptr<sink<uint64_t>> make_sink<uint64_t>(const session_context& constext, size_t segment_id, sink_type type, size_t channel_index);
