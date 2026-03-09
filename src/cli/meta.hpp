#pragma once

#include <array>
#include <string_view>
#include <span>
#include <algorithm>
#include <iterator>
#include <utility>
#include <source_location>

#include "common/meta.hpp"
#include "utility.hpp"

namespace meta {

	template <typename T>
	using enumerator_mapping_item_t = std::pair<T, std::string_view>;

	template <typename T, size_t N>
	using enumerator_mapping_description_t = std::array<enumerator_mapping_item_t<T>, N>;

	// someday it will be possible to reflect on enums in C++26
	template <typename T>
	struct enumerators_mapping;


	template <size_t N>
	using member_names_description_t = std::array<std::string_view, N>;

	template <typename T>
	struct member_names_trait;


	template <auto value, typename TraitT = member_names_trait<decltype(value)>>
	struct as_string {
		static consteval std::string_view parse() {
			constexpr std::string_view raw = std::source_location::current().function_name();
			constexpr std::string_view struct_name = "as_string"sv;
			constexpr std::string_view tail = raw.substr(
				std::distance(raw.cbegin(),
					std::next(
						std::search(raw.cbegin(), raw.cend(), struct_name.cbegin(), struct_name.cend()),
						struct_name.size())));

			constexpr auto value_begin = std::find(tail.cbegin(), tail.cend(), '{');
			constexpr size_t value_end_pos = matching_char_position(tail);
			constexpr size_t value_start_pos = std::distance(tail.cbegin(), value_begin) + 1;

			// TODO: PATCHME: static is not needed here per the standard.
			// but it doesn't compile otherwise on clang (due to lambda capture handling)
			//
			// But then it doesn't compile under msvc, because per the standard static variable
			// declarations are not allowed in constexpr functions (clang compiled it with no errors
			// as C++23 extension)
			// So capture default is added to lambda below
			// static 
			constexpr std::string_view values = tail.substr(value_start_pos, value_end_pos - value_start_pos);
			// static 
			constexpr auto token_collection = tokenize<std::tuple_size_v<decltype(TraitT::names)>>(values);


			constexpr auto merger = [=]<size_t first, size_t... args>(std::index_sequence<first, args...>) consteval {
				constexpr auto item_handler = [=]<size_t index>() consteval {
					constexpr std::span name_value_joiner = trim_terminator(std::span{ " = " });
					constexpr auto name_ss = make_static_string<TraitT::names[index].size()>(TraitT::names[index]);
					constexpr auto joiner_ss = make_static_string(name_value_joiner);
					constexpr auto value_ss = make_static_string<token_collection[index].second>(values.substr(
						token_collection[index].first,
						token_collection[index].second));

					return name_ss + joiner_ss + value_ss;
				};
				constexpr auto separator = make_static_string(trim_terminator(std::span{ ", " }));
				return ((item_handler.template operator()<first>()) + ... +
					(separator + item_handler.template operator()<args>()));
			};

			constexpr auto value_open = make_static_string(trim_terminator(std::span{ "{" }));
			constexpr auto value_close = make_static_string(trim_terminator(std::span{ "}" }));

			constexpr auto value_description = value_open +
				merger(std::make_index_sequence<std::tuple_size_v<decltype(TraitT::names)>>{}) +
				value_close;

			return materialize<value_description>();
		}
	};
}