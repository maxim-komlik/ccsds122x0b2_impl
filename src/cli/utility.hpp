#pragma once

#include <tuple>
#include <type_traits>
#include <string_view>
#include <span>
#include <array>
#include <algorithm>
#include <source_location>

#include "common/utils.hpp"


// tuple metaprogramming helpers. move?
//
// similar to std::tuple_cat, but template-parameters-based type transform only
template <typename T1, typename T2>
struct tuple_merge;

template <typename... Ts>
struct tuple_merge<std::tuple<Ts...>, std::tuple<>> {
private:
	using subject_t = std::tuple<Ts...>;

public:
	using type = subject_t;
};

template <typename T2, typename... Ts>
struct tuple_merge<std::tuple<Ts...>, T2> {
private:
	static_assert(is_tuple_v<T2> == true);

	using head = std::tuple<Ts...>;
	using tail = T2;

public:
	using type = tuple_merge<
		tuple_append_element_t<
			head,
			tuple_element_first_t<tail>>,
		tuple_remove_first_element_t<tail>>::type;
};

template <typename T1, typename T2>
using tuple_merge_t = tuple_merge<T1, T2>::type;


template <typename T, template <typename> typename Decorator>
struct tuple_decorate_elements;

template <template <typename> typename Decorator>
struct tuple_decorate_elements<std::tuple<>, Decorator> {
private:
	using subject_t = std::tuple<>;

public:
	using type = subject_t;
};

template <template <typename> typename Decorator, typename... Ts>
struct tuple_decorate_elements<std::tuple<Ts...>, Decorator> {
private:
	using subject_t = std::tuple<Ts...>;
	using next_t = tuple_remove_first_element_t<subject_t>;

public:
	using type = tuple_append_element_t<
		typename tuple_decorate_elements<
		next_t,
		Decorator>::type,
		typename Decorator<tuple_element_first_t<subject_t>>::type>;
};

template <typename T, template <typename> typename Decorator>
using tuple_decorate_elements_t = tuple_decorate_elements<T, Decorator>::type;


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


namespace meta {
	using namespace std::literals;

	template <auto value>
	struct static_member_wrapper {
		static constexpr auto wrapped = value;
	};


	template <size_t N>
	consteval std::span<const char, N - 1> trim_terminator(std::span<const char, N> value) {
		return value.first<N - 1>();
	}


	template <size_t N>
	struct static_string;

	// avoid defining constructors for static_string to make it structural/literal type
	template <size_t N>
	consteval static_string<N> make_static_string(std::span<const char, N> source);

	template <size_t N>
	consteval static_string<N> make_static_string(std::span<const char, N> source);


	template <size_t N>
	struct static_string {
		std::array<char, N + 1> value = { '\0' };

		static constexpr size_t size() { return N; }

		template <char space = ' '>
		consteval size_t count_leading_spaces() const {
			return std::distance(this->value.begin(), 
				std::find_if(this->value.cbegin(), std::next(this->value.cbegin(), N),
					[](const char& item) -> bool { return item != space; }));
		}

		template <size_t M>
		consteval static_string<N - M> substr() const {
			return make_static_string(std::span(this->value).subspan<M>().first<N - M>());
		}

		template <size_t M>
		consteval static_string<M> first() const {
			return make_static_string(std::span(this->value).first<M>());
		}

		template <size_t M>
		consteval static_string<M> last() const {
			return make_static_string(std::span(this->value).last<M + 1>().first<M>());
		}
	};

	template <size_t N>
	consteval static_string<N> make_static_string(std::string_view source) {
		using result_t = static_string<N>;
		result_t result;

		if (source.size() < N) {
			throw "blame yourself!";
		}

		std::copy_n(source.begin(), N, result.value.begin());
		return result;
	}

	template <size_t N>
	consteval static_string<N> make_static_string(std::span<const char, N> source) {
		using result_t = static_string<N>;
		result_t result;
		std::copy_n(source.begin(), N, result.value.begin());
		return result;
	}

	template <size_t N, size_t M>
	consteval static_string<N + M> operator+(static_string<N> lhs, static_string<M> rhs) {
		using result_t = static_string<N + M>;

		result_t result;
		std::copy_n(lhs.value.begin(), N, result.value.begin());
		std::copy_n(rhs.value.begin(), M, std::next(result.value.begin(), N));
		return result;
	}

	template <size_t... ArgN>
	consteval static_string<(ArgN + ...)> concat(static_string<ArgN>... args) {
		return (args + ...);
	}


	template <auto value>
	consteval std::string_view materialize() {
		static_assert(std::is_same_v<static_string<value.size()>, decltype(value)>);

		using wrapper_t = static_member_wrapper<value.value>;
		return std::string_view{ wrapper_t::wrapped.data(), wrapper_t::wrapped.size() - 1 };
	}


	template <typename T>
	consteval static_string<std::numeric_limits<T>::digits10 + 2> to_static_string(T value, int base = 10) {
		constexpr size_t buffer_size = std::numeric_limits<T>::digits10 + 2; // exact digits, last inexact digit and sign
		std::array<char, buffer_size> buffer = {};

		bool put_sign = value < 0;

		ptrdiff_t pos = buffer.size() - 1;
		for (; pos > 0; --pos) {
			// std::abs is not constexpr until C++23
			char digit = magnitude(value % base);
			value /= base;

			buffer[pos] = '0' + digit;

			if (value == 0) {
				break;
			}
		}

		buffer[--pos] = '-' & (-(int)(put_sign));
		pos -= put_sign;
		//buffer[pos] = char{};	// overrides '-' for positive numbers

		// constexpr char8_t zws_char[] = { 0xE2, 0x80, 0x8B };

		return make_static_string(std::span(std::as_const(buffer)));
	}

	template <typename T, int base = 10>
	consteval auto to_static_string() {
		static_assert(std::is_same_v<T, std::integral_constant<typename T::value_type, T::value>>);

		constexpr auto untrimmed = to_static_string(T::value, base);
		constexpr size_t trim_count = untrimmed.count_leading_spaces<'\0'>();
		return untrimmed.substr<trim_count>();
	}


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

			constexpr std::string_view values = tail.substr(value_start_pos, value_end_pos - value_start_pos);

			constexpr auto token_collection = tokenize<std::tuple_size_v<decltype(TraitT::names)>>(values);


			constexpr auto merger = []<size_t first, size_t... args>(std::index_sequence<first, args...>) consteval {
				constexpr auto item_handler = []<size_t index>() consteval {
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

			// constexpr auto value_extended = make_static_string<values.size()>(values) + value_description;
			// return materialize<value_extended>();
			return materialize<value_description>();
		}
	};
}
