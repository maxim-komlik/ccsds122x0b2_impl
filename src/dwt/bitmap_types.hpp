#pragma once

#include <array>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>
#include <cstddef>

// TODO: the staff below is kind of sufficient_integral

template <size_t bdepth, bool img_is_signed = true>
struct bitmap_type_params;


//
// unsigned bitmaps

template <>
struct bitmap_type_params<7> {
	using type = int8_t;
};

template <>
struct bitmap_type_params<15> {
	using type = int16_t;
};

template <>
struct bitmap_type_params<31> {
	using type = int32_t;
};

template <>
struct bitmap_type_params<63> {
	using type = int64_t;
};


//
// unsigned bitmaps

template <>
struct bitmap_type_params<8, false> {
	using type = uint8_t;
};

template <>
struct bitmap_type_params<16, false> {
	using type = uint16_t;
};

template <>
struct bitmap_type_params<32, false> {
	using type = uint32_t;
};

template <>
struct bitmap_type_params<64, false> {
	using type = uint64_t;
};


template <bool is_signed>
struct bdepth_bitmap_catalog;

template <>
struct bdepth_bitmap_catalog<true> {
	static constexpr std::array<size_t, 4> values = { 7, 15, 31, 63 };
};

template <>
struct bdepth_bitmap_catalog<false> {
	static constexpr std::array<size_t, 4> values = { 8, 16, 32, 64 };
};


// excessive use of template below is discouraged, prefer sufficient_integral whenever applicable
template <size_t bit_depth, bool if_signed>
struct sufficient_depth_integral {
	static_assert(bit_depth <= bdepth_bitmap_catalog<if_signed>::values.back());

	static constexpr size_t depth = std::invoke([]() constexpr -> size_t {
			auto& values = bdepth_bitmap_catalog<if_signed>::values;
			return *std::lower_bound(values.cbegin(), values.cend(), bit_depth);
		});
	using type = bitmap_type_params<depth, if_signed>::type;
};

template <size_t bit_depth, bool if_signed>
using sufficient_depth_integral_t = sufficient_depth_integral<bit_depth, if_signed>::type;


template <template <typename T> typename Handler>
struct integral_bit_depth_dispatch{

	template <typename... Args>
	static void apply(size_t bit_depth, bool if_signed, Args&&... args) {
		dispatch_signed(bit_depth, if_signed, std::forward<Args>(args)...);
	}

private:
	template <bool if_signed, typename... Args>
	static void dispatch_bit_depth(size_t bit_depth, Args&&... args) {
		auto& values = bdepth_bitmap_catalog<if_signed>::values;
		size_t target_bit_depth = *std::lower_bound(values.cbegin(), values.cend(), bit_depth);

		auto target_invoke = [&args...]<typename T>() -> void {
			std::invoke(Handler<T>{}, std::forward<Args>(args)...);
		};

		constexpr size_t catalog_size = std::tuple_size_v<decltype(bdepth_bitmap_catalog<if_signed>::values)>;
		std::invoke([&target_invoke, target_bit_depth]<size_t... indices>(std::index_sequence<indices...>) -> void {
				auto item_handler = [&target_invoke, target_bit_depth]<size_t index>() -> void {
					constexpr size_t static_bit_depth = bdepth_bitmap_catalog<if_signed>::values[index];
					if(target_bit_depth == static_bit_depth) {
						using param_t = bitmap_type_params<static_bit_depth, if_signed>::type;
						target_invoke.template operator()<param_t>();
					}
				};

				((item_handler.template operator()<indices>()), ...);
			}, 
			std::make_index_sequence<catalog_size>{});
	}

	template <typename... Args>
	static void dispatch_signed(size_t bit_depth, bool if_signed, Args&&... args) {
		if (if_signed) {
			dispatch_bit_depth<true, Args...>(bit_depth, std::forward<Args>(args)...);
		} else {
			dispatch_bit_depth<false, Args...>(bit_depth, std::forward<Args>(args)...);
		}
	}

};
