#pragma once

#include <functional>

enum UnrollFoldType {
	left,
	right
};

template <UnrollFoldType UT, size_t N>
struct _unroll {
	template <typename FT, typename T, typename ... Args>
	inline static void apply(FT func, T param, Args ... args) {
		static_assert(N == sizeof...(Args));
		if constexpr (UT == left) {
			_unroll<UT, N - 1>::apply(func, args...);
		}
		func.template operator()<N>(param);
		if constexpr (UT == right) {
			_unroll<UT, N - 1>::apply(func, args...);
		}
	}
};

template <UnrollFoldType UT>
struct _unroll <UT, 0> {
	template <typename FT, typename T>
	inline static void apply(FT func, T param) {
		func.template operator()<0>(param);
	}
};

template <UnrollFoldType UT, size_t N>
struct _unroll_init {
	template <typename FT1, typename FT2, typename T, typename ... Args>
	inline static void apply(FT1 load, FT2 link, T param, Args ... args) {
		static_assert(sizeof...(args) >= 1);
		static_assert(N == sizeof...(Args));
		if constexpr (UT == left) {
			_unroll<UT, N - 1>::apply(
				[load, link]<size_t lN>(T lparam) {
					load.template operator() < lN > (lparam);
					link();
				}, args...);
		}
		load.template operator()<N>(param);
		if constexpr (UT == right) {
			_unroll<UT, N - 1>::apply(
				[load, link]<size_t lN>(T lparam) {
					link();
					load.template operator() < lN > (lparam);
				}, args...);
		}
	}
};

template <bool Init = false, UnrollFoldType UT = left>
struct _unroll_wrapper {
	template <typename FT1, typename ... Args>
	inline static void apply(FT1 func, Args ... args) {
		_unroll<UT, sizeof...(Args) - 1>::apply(func, args...);
	}
};

template <UnrollFoldType UT>
struct _unroll_wrapper<true, UT> {
	template <typename FT1, typename FT2, typename ... Args>
	inline static void apply(FT1 load, FT2 link, Args ... args) {
		_unroll_init<UT, sizeof...(Args) - 1>::apply(load, link, args...);
	}
};


template <bool B = false, UnrollFoldType UT = left>
using unroll = _unroll_wrapper<B, UT>;


// template <typename T>
// inline T altitude(T val) {
// 	constexpr size_t bitsize = (sizeof(T) << 3) - 1;
// 	return (val ^ (val >> bitsize)) - (val >> bitsize);
// }
// 
// template <typename T>
// inline T relu(T val) {
// 	constexpr size_t bitsize = (sizeof(T) << 3) - 1;
// 	return val & (~(val >> bitsize));
// }
