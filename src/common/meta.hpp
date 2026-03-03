#pragma once

#include <string_view>
#include <tuple>
#include <span>
#include <algorithm>
#include <type_traits>
#include <utility>
#include <limits>
#include <cstddef>

#include "utility.hpp"


namespace meta {

// Below come heavy metaprogramming utils. Maybe makes sense to put them to dedicated namespace one day.

template <typename R, typename... ParamsT>
struct plain_fn {
	using type = R(ParamsT...);
};

// Lord have mercy on me!
// Aside from return type and argument types, function type includes cv-qualifiers, 
// reference and exception qualifiers. Hence we need to provide 23 specializations.
//

template <typename T>
struct remove_fn_qualifiers {
	using type = T;
};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) volatile> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const volatile> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) &> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const &> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) volatile &> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const volatile &> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) &&> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const &&> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) volatile &&> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const volatile &&> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) volatile noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const volatile noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) & noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const & noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) volatile & noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const volatile & noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) && noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const && noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) volatile && noexcept> : plain_fn<R, ParamsT...> {};

template <typename R, typename... ParamsT>
struct remove_fn_qualifiers <R(ParamsT...) const volatile && noexcept> : plain_fn<R, ParamsT...> {};

template <typename T>
using remove_fn_qualifiers_t = remove_fn_qualifiers<T>::type;


// The only things that can be done to member function id-expr is member access or 
// member pointer instantiation. The first does not contain function type info. The 
// latter contains the info, but is pointer itself. The utility below allows to 
// extract function type from function member pointer type.
//

template <typename T>
struct remove_mem_pointer {
	using type = T;
};

template <typename C, typename T>
struct remove_mem_pointer<T(C::*)> {
	using type = T;
};

template <typename T>
using remove_mem_pointer_t = remove_mem_pointer<T>::type;

template <typename T>
struct remove_rvalue_reference {
	using type = T;
};

template <typename T>
struct remove_rvalue_reference <T&&> {
	using type = T;
};

template <typename T>
using remove_rvalue_reference_t = remove_rvalue_reference<T>::type;


template <typename CallableT>
struct _arguments_types {
private:
	// Primary template assumes CallableT is a callable object, i.e. has call 
	// operator defined. Get call operator type info via member pointer.
	using call_mem_ptr_t = decltype(&CallableT::operator());

public:
	// fall back to function type specialization
	using types = _arguments_types<
			remove_fn_qualifiers_t<remove_mem_pointer_t<call_mem_ptr_t>>>::types;
};

template <typename R, typename... ParamsT>
struct _arguments_types <R(ParamsT...)> {
	using types = std::tuple<remove_rvalue_reference_t<ParamsT>...>;
};

template <typename T>
using call_arguments_types = 
		_arguments_types<remove_fn_qualifiers_t<std::remove_cvref_t<T>>>::types;


template <typename... Ts>
struct tuple_wrap {
	using type = std::tuple<Ts...>;
};

template <typename... Ts>
struct tuple_wrap<std::tuple<Ts...>> {
	using type = std::tuple<Ts...>;
};

template <typename T>
class is_tuple: public std::false_type { };

template <typename... Ts>
class is_tuple<std::tuple<Ts...>>: public std::true_type { };

template <typename T>
struct tuple_element_first;

template <typename... Ts>
struct tuple_element_first<std::tuple<Ts...>> {
private:
	using subject_t = std::tuple<Ts...>;
public:
	using type = std::tuple_element_t<0, subject_t>;
};

template <typename T, typename D>
struct tuple_prepend_element;

template <typename T, typename... Ts>
struct tuple_prepend_element<std::tuple<Ts...>, T> {
	using type = std::tuple<T, Ts...>;
};

template <typename T>
struct tuple_remove_first_element;

template <typename T, typename... Ts>
struct tuple_remove_first_element<std::tuple<T, Ts...>> {
	using type = std::tuple<Ts...>;
};

template <typename T, typename D>
struct tuple_replace_first_element;

template <typename T, typename... Ts>
struct tuple_replace_first_element<std::tuple<Ts...>, T> {
private:
	using subject_t = std::tuple<Ts...>;
public:
	using type = tuple_prepend_element<typename tuple_remove_first_element<subject_t>::type, T>::type;
};

namespace details {
	template<bool last, size_t count, typename T>
	struct tuple_remove_n_last_elements_impl;

	template<bool last, size_t count, typename... Ts>
	struct tuple_remove_n_last_elements_impl<last, count, std::tuple<Ts...>> {
	private:
		using subject_t = std::tuple<Ts...>;
		using next_t = tuple_remove_first_element<subject_t>::type;
	public:
		using type = tuple_prepend_element<
			typename tuple_remove_n_last_elements_impl<(std::tuple_size_v<next_t> <= count), count, next_t>::type,
			typename tuple_element_first<subject_t>::type>::type;
	};

	template<size_t count, typename... Ts>
	struct tuple_remove_n_last_elements_impl<true, count, std::tuple<Ts...>> {
	public:
		using type = std::tuple<>;
	};

	template <typename T, size_t count>
	using tuple_remove_n_last_elements_impl_t = tuple_remove_n_last_elements_impl<
		(std::tuple_size_v<T> <= count), 
		count, 
		T>::type;
}

template <typename T>
struct tuple_element_last;

template <typename T, typename... Ts>
struct tuple_element_last<std::tuple<T, Ts...>> {
private:
	using subject_t = std::tuple<T, Ts...>;
public:
	using type = std::tuple_element_t<std::tuple_size_v<subject_t> - 1, subject_t>;
};

template <typename T, typename D>
struct tuple_append_element;

template <typename T, typename... Ts>
struct tuple_append_element<std::tuple<Ts...>, T> {
	using type = std::tuple<Ts..., T>;
};

template <typename T>
struct tuple_remove_last_element;

template <typename... Ts>
struct tuple_remove_last_element<std::tuple<Ts...>> {
private:
	using subject_t = std::tuple<Ts...>;
public:
	// using type = std::tuple<Ts...>;
	using type = details::tuple_remove_n_last_elements_impl_t<subject_t, 1>;
};

template <typename T, typename D>
struct tuple_replace_last_element;

template <typename T, typename... Ts>
struct tuple_replace_last_element<std::tuple<Ts...>, T> {
private:
	using subject_t = std::tuple<Ts...>;
public:
	using type = tuple_append_element<typename tuple_remove_last_element<subject_t>::type, T>::type;
};

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
		typename tuple_append_element<
			head,
			typename tuple_element_first<tail>::type>::type,
		typename tuple_remove_first_element<tail>::type>::type;
};


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
	using next_t = tuple_remove_first_element<subject_t>::type;

public:
	using type = tuple_append_element<
		typename tuple_decorate_elements<
		next_t,
		Decorator>::type,
		typename Decorator<typename tuple_element_first<subject_t>::type>::type>::type;
};


// helpers

template <typename... Ts>
using tuple_wrap_t = tuple_wrap<Ts...>::type;

template <typename T>
constexpr bool is_tuple_v = is_tuple<T>::value;

template <typename T>
using tuple_element_last_t = tuple_element_last<T>::type;

template <typename T, typename D>
using tuple_append_element_t = tuple_append_element<T, D>::type;

template <typename T>
using tuple_remove_last_element_t = tuple_remove_last_element<T>::type;

template <typename T, typename D>
using tuple_replace_last_element_t = tuple_replace_last_element<T, D>::type;

template <typename T>
using tuple_element_first_t = tuple_element_first<T>::type;

template <typename T, typename D>
using tuple_prepend_element_t = tuple_prepend_element<T, D>::type;

template <typename T>
using tuple_remove_first_element_t = tuple_remove_first_element<T>::type;

template <typename T, typename D>
using tuple_replace_first_element_t = tuple_replace_first_element<T, D>::type;

template <typename T1, typename T2>
using tuple_merge_t = tuple_merge<T1, T2>::type;

template <typename T, template <typename> typename Decorator>
using tuple_decorate_elements_t = tuple_decorate_elements<T, Decorator>::type;


// constexpr string metaprogramming utilities

using namespace std::literals;

template <size_t N>
consteval std::span<const char, N - 1> trim_terminator(std::span<const char, N> value) {
	return value.first<N - 1>();
}

template <auto value>
struct static_member_wrapper {
	static constexpr auto wrapped = value;
};


template <size_t N>
struct static_string;


template <auto value>
consteval std::string_view materialize() {
	static_assert(std::is_same_v<static_string<value.size()>, decltype(value)>);

	using wrapper_t = static_member_wrapper<value.value>;
	return std::string_view{ wrapper_t::wrapped.data(), wrapper_t::wrapped.size() - 1 };
}


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

}
