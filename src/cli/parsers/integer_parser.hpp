#pragma once

#include <string_view>
#include <vector>
#include <charconv>
#include <limits>
#include <cstddef>

#include "cli.hpp"
#include "expected.hpp"
#include "utility.hpp"

namespace cli::parsers {

template <typename T, T max = std::numeric_limits<T>::max(), T min = std::numeric_limits<T>::min(), int base = 10>
struct integer_parser_impl {
	static_assert (min <= max);

	using value_t = T;

	cli::expected<T> parse(std::vector<std::string_view>& tokens) {
		T result;
		std::string_view token = tokens.back();
		auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), result, base);

		bool valid = true;
		valid &= (ec == std::errc{});
		valid &= (ptr == (token.data() + token.size()));
		if (!valid) {
			return cli::unexpected{ std::errc::invalid_argument, invalid_parameter_description };
		}

		valid &= (result <= max);
		valid &= (result >= min);
		if (!valid) {
			return cli::unexpected{ std::errc::protocol_error };
		}

		tokens.pop_back();
		return result;
	}

private:
	static consteval std::string_view generate_requirements() {
		constexpr auto span_wrapper = []<size_t N>(std::span<const char, N> obj) consteval {
			return meta::make_static_string(obj.first<N - 1>());
		};

		constexpr std::span general_p1 = "Parameter value must be ";
		constexpr std::span unsigned_spec = "unsigned ";
		constexpr std::span general_p2 = "integer, specified in base ";
		constexpr std::span general_p3 = ", within range [";
		constexpr std::span separator = ", ";
		constexpr std::span general_p4 = "]. ";

		constexpr auto base_desc = meta::to_static_string<std::integral_constant<int, base>>();
		constexpr auto min_desc = meta::to_static_string<std::integral_constant<T, min>>();
		constexpr auto max_desc = meta::to_static_string<std::integral_constant<T, max>>();

		// better be std::invoke, but doesn't compile for some reason under msvc
		constexpr size_t prefix_count = []() consteval {
			size_t result = general_p1.size() - 1;
			if (std::is_unsigned_v<T>) {
				result += unsigned_spec.size();
			}
			return result;
		}();

		return meta::materialize<
			(span_wrapper(general_p1) + span_wrapper(unsigned_spec)).first<prefix_count>() +
			span_wrapper(general_p2) + base_desc + span_wrapper(general_p3) + 
			min_desc + span_wrapper(separator) + max_desc + span_wrapper(general_p4)>();
	}

	static consteval std::string_view generate_placeholder() {
		using namespace std::literals;
		if constexpr (std::is_unsigned_v<T>) {
			return "<uint>"sv;
		}

		return "<int>"sv;
	}

	static constexpr std::string_view invalid_parameter_description = "Couldn't parse number parameter. "sv;

public:
	static constexpr std::string_view requirements = generate_requirements();
	static constexpr std::string_view placeholder = generate_placeholder();
};

template <ptrdiff_t max = std::numeric_limits<ptrdiff_t>::max(), ptrdiff_t min = std::numeric_limits<ptrdiff_t>::min()>
using integer_parser = integer_parser_impl<ptrdiff_t, max, min>;

template <size_t max = std::numeric_limits<size_t>::max(), size_t min = std::numeric_limits<size_t>::min()>
using unsigned_integer_parser = integer_parser_impl<size_t, max, min>;

}
