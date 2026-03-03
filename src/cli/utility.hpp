#pragma once

#include <string>
#include <array>
#include <string_view>
#include <span>
#include <algorithm>
#include <charconv>
#include <iostream>
#include <utility>
#include <limits>
#include <cstddef>

#include "exception/exception.hpp"


template <char open = '{', char close = '}'>
constexpr size_t matching_char_position(std::string_view str) {
	auto pos = str.cbegin();
	ptrdiff_t open_count = 0;
	do {
		auto next_pos = std::find(pos, str.cend(), close);

		open_count += std::count(pos, next_pos, open);
		pos = next_pos + (next_pos != str.cend());
	} while (--open_count > 0);

	return pos - str.cbegin() - (pos != str.cend());
}

template <size_t N, char separator = ','>
constexpr std::array<std::pair<size_t, size_t>, N> tokenize(std::string_view src) {
	using result_t = std::array<std::pair<size_t, size_t>, N>;
	result_t result;

	auto begin = src.cbegin();

	for (ptrdiff_t i = 0; i < N; ++i) {
		auto end = std::find(begin, src.cend(), separator);
		size_t begin_pos = std::distance(src.cbegin(), begin);
		size_t end_pos = std::distance(begin, end);
		size_t embedded_begin_pos = std::distance(begin, std::find(begin, end, '{'));
		if (embedded_begin_pos < end_pos) {
			std::string_view embedded_region = src.substr(begin_pos);
			end_pos = matching_char_position(embedded_region);
			end_pos += (end_pos != embedded_region.size());
			end = std::next(begin, end_pos);
		}
		result[i] = std::pair{ begin_pos, end_pos };
		std::advance(begin, end_pos + (end != src.cend()));
	}

	return result;
}

template <typename T>
inline std::u8string to_u8string(T integer) {
	// God have mercy on me!
	// because of lack of utf-8 support by the [current?] C++ standard

	constexpr size_t buffer_size = std::numeric_limits<T>::digits10 + 2; // exact digits, last inexact digit and sign
	std::array<char, buffer_size> buffer = {};

	// ascii subset is conversion-compatible in utf-8; new string is made, therefore no leading escape sequences
	auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), integer);
	if (ec != std::errc{}) {
		// and what now? it is never supposed to happen
		// std::unreachable?
	}

	// TODO: strinct aliasing? Or provides storage and therefore ok?
	std::u8string_view int_view{ 
		reinterpret_cast<char8_t*>(buffer.data()), 
		(std::u8string_view::size_type)(ptr - buffer.data()) 
	};
	return static_cast<std::u8string>(int_view);
}

inline std::ostream& operator<<(std::ostream& os, std::u8string_view sv) {
	auto if_utf8_locale_name = [](std::string_view loc_view) -> bool {
		using namespace std::literals;
		constexpr std::array candidate_postfixes = {
			".UTF-8"sv,
			".utf-8"sv,
			".UTF8"sv, 
			".utf8"sv
		};

		auto impl = [&]<size_t... indices>(std::index_sequence<indices...>) -> bool {
			auto item_handler = [&]<size_t index>() -> bool {
				return loc_view.find(std::get<index>(candidate_postfixes)) != loc_view.npos;
			};

			return ((item_handler.template operator()<indices>()) | ...);
		};
		
		bool result = impl(std::make_index_sequence<std::tuple_size_v<decltype(candidate_postfixes)>>());

		// or if neither of the above match, check if it is classic locale, because it is guaranteed (?)
		// to be utf-8 aware due to linker configurations
		//
		// all these checks meant to cope with erroneous or external locale modifications, but make little 
		// sense anyway due to inheritenly prone to race conditions locale api
		constexpr auto classic_locale_name = "C"sv;
		return result | (loc_view == classic_locale_name);
	};

	// what really needed here is C++26 std::text_encoding
	if (!if_utf8_locale_name(os.getloc().name())) {
		throw cli::envrironment_not_supported();
	}

	// no UB because char is used
	std::string_view locale_dependent{ reinterpret_cast<const char*>(sv.data()), sv.size() };
	return (os << locale_dependent);
}
