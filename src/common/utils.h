#pragma once

#include <functional>
#include <bit>

// loop unroll staff

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

// common type wrappers and helpers

// TODO: add specializations for dense instead of fast
// specify types for input size not equal to napot

template <size_t size_bytes>
struct _sufficient_integral;

template <>
struct _sufficient_integral<2> {
	typedef int_fast16_t type;
};

template <>
struct _sufficient_integral<4> {
	typedef int_fast32_t type;
};

template <>
struct _sufficient_integral<8> {
	typedef int_fast64_t type;
};

template <typename T>
using sufficient_integral = _sufficient_integral<sizeof(T)>::type;

template <size_t byte_width>
using sufficient_integral_i = _sufficient_integral<byte_width>::type;


template <typename T>
class Finalizer {
private:
	friend T;
	~Finalizer() = default;
};

// general util functions

// Copy-swap for strong exception safety
template <typename T>
T& strong_assign(T& dst, T src) {
	using std::swap;
	swap(dst, src);
	return dst;
}

// Vectorizable alternative for abs
template <typename T>
inline std::make_unsigned_t<T> magnitude(T val) {
	constexpr size_t bitsize = (sizeof(T) << 3) - 1;
	return (val ^ (val >> bitsize)) - (val >> bitsize);
}

template <typename T>
inline T signxor(T val) {
	constexpr size_t bitsize = (sizeof(T) << 3) - 1;
	return (val ^ (val >> bitsize));
}

template <typename T>
inline T signext(T val, size_t sign_pos) {
	T mask = ((T)(-1)) << (sign_pos - 1);
	return (val & (~mask)) - (val & mask);
	// return val | ((((T)(-1)) << (sign_pos - 1)) + val);
}

// TODO: apply signed constraint (uses sign extension)
// Currently fixed via make_unsigned, therefore input
// value is limited to ((1 << (sizeof(T) << 3) - 1) - 1)
// in unsigned representation (not valid for negative ints)
// 
// constexpr is implicitly inline
template <typename T>
constexpr T relu(T val) {
	using sT = std::make_signed_t<T>;
	constexpr size_t bitsize = (sizeof(sT) << 3) - 1;
	return val & (~(((sT)(val)) >> bitsize));
}

// Requires src to be large enough to handle *length* elements 
// + rest of alignment unit at the end. Otherwise blame yourself.
template <typename T, size_t alignment>
size_t bdepthv(T* src, size_t length) {
	T buffer[alignment] = { 0 };

	for (size_t i = 0; i < length; i += alignment) {
		for (size_t j = 0; j < alignment; ++j) {
			// TODO: check handling negative power-of-two
			// call below is actually abs
			buffer[j] |= magnitude<T>(src[i + j]);
		}
	}

	for (size_t i = alignment >> 1; i > 0; i >>= 1) {
		for (size_t j = 0; j < i; ++j) {
			buffer[j] |= buffer[i + j];
		}
	}

	// TODO: std::bit_width
	// size_t depth = sizeof(*buffer) << 3;
	// size_t i = ((size_t)-1) << (depth - 1);
	// while (!(i & buffer[0])) {
	// 	i >>= 1;
	// 	--depth;
	// }

	// size_t depth = ((sizeof(i) << 3) - 1);
	// for (size_t i = ((-1) << depth); !(i & buffer); i >>= 1, --depth) { }
	// return depth;
	return std::bit_width((std::make_unsigned_t<T>)(*buffer));
}

// accumulate with or vectorized
// 
// Requires src to be large enough to handle *length* elements 
// + rest of alignment unit at the end. Otherwise blame yourself.
template <typename T, size_t alignment>
inline std::make_unsigned_t<T> accorv(const T* src, size_t length) {
	std::make_unsigned_t<T> buffer[alignment] = { 0 };

	for (size_t i = 0; i < length; i += alignment) {
		for (size_t j = 0; j < alignment; ++j) {
			buffer[j] |= magnitude(src[i + j]);
		}
	}

	for (size_t i = alignment >> 1; i > 0; i >>= 1) {
		for (size_t j = 0; j < i; ++j) {
			buffer[j] |= buffer[i + j];
		}
	}

	return buffer[0];
}