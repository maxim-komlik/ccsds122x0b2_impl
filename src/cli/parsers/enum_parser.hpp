#pragma once

#include <string_view>
#include <vector>
#include <array>
#include <utility>

#include "expected.hpp"
#include "common/utils.hpp"
#include "utility.hpp"

template <typename T>
struct enum_parser {
	using value_t = T;

private:
	using mapping_t = meta::enumerators_mapping<T>;

public:
	cli::expected<T> parse(std::vector<std::string_view>& tokens) {
		std::string_view token = tokens.back();
		bool enumerator_found = false;
		T result;

		std::apply(
			[&](auto... args) -> void {
				unroll<>::apply(
					[&]<size_t index>(auto) -> void {
						if (token == enum_parser::get_mapping<index>().second) {
							result = enum_parser::get_mapping<index>().first;
							enumerator_found = true;
						}
					}, 
					args...);
			}, 
			mapping_t::description);
		if (!enumerator_found) {
			return cli::unexpected{ std::errc::invalid_argument, invalid_parameter_description };
		}

		tokens.pop_back();
		return result;
	}

private:
	template <size_t index>
	static consteval meta::enumerator_mapping_item_t<T> get_mapping() {
		return std::get<index>(mapping_t::description);
	}

	static consteval std::string_view generate_requirements() {
		// for some reason, lambda calls via std::invoke fail to compile in constexpr context
		// under msvc, hence private static function

		constexpr auto general_p1 = meta::make_static_string(meta::trim_terminator(
			std::span{ "Parameter value must be one of the following: " }));
		constexpr auto general_p2 = meta::make_static_string(meta::trim_terminator(std::span{ ". " }));

		// for some reason, separator is not captured when defined as constexpr std::span 
		// under msvc
		constexpr auto separator = meta::make_static_string(meta::trim_terminator(std::span{ ", " }));

		constexpr auto enumerators_generator = 
			[]<size_t first, size_t... args>(std::index_sequence<first, args...>) consteval {
				constexpr auto sv_wrapper = []<size_t index>() consteval {
					constexpr std::string_view target = get_mapping<index>().second;
					return meta::make_static_string<target.size()>(target);
				};

				return ((sv_wrapper.template operator()<first>()) + ... + 
					(separator + sv_wrapper.template operator()<args>()));
			};

		constexpr auto enumerators = enumerators_generator(std::make_index_sequence<mapping_t::description.size()>());

		return meta::materialize<general_p1 + enumerators + general_p2>();
	}

	static consteval std::string_view generate_placeholder() {
		using namespace std::literals;
		return "<enum>"sv;
	}

	static constexpr std::string_view invalid_parameter_description = "Couldn't parse number parameter. ";

public:
	static constexpr std::string_view requirements = generate_requirements();
	static constexpr std::string_view placeholder = generate_placeholder();

	template <value_t value>
	static consteval std::string_view get_enumerator_string() {
		for (ptrdiff_t i = 0; i < mapping_t::description.size(); ++i) {
			if (value == mapping_t::description[i].first) {
				return mapping_t::description[i].second;
			}
		}

		throw "No mapping for enumerator!";
	}
};
