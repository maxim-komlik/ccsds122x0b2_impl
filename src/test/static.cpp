#include "gtest/gtest.h"

#include <array>
#include <tuple>
#include <vector>
#include <functional>

#include "utils.hpp"
#include "bpe/bpe.tpp"

TEST(experimental, DISABLED_quantizationValueCheck) {
	typedef ptrdiff_t item_t;

	std::function<item_t(item_t, item_t)> testFunctions[] = {
		[](item_t a, item_t bdepth) -> item_t {
			if (bdepth <= 3) {
				a = 0;
			} else if ((bdepth - a <= 1) && (bdepth > 3)) {
				a = bdepth - 3;
			} else if ((bdepth  - a > 10) && (bdepth > 3)) {
				a = bdepth - 10;
			}
			// 4.3.1.3, make sure that result is not less that BitShift(LL3) = 3
			return std::max<decltype(a)>(a, 3);
		},
		// simplified
		[](item_t a, item_t bdepth) -> item_t {
			if (bdepth - a <= 1) {
				a = bdepth - 3;
			} else if (bdepth - a > 10) {
				a = bdepth - 10;
			}
			return std::max<decltype(a)>(a, 3);
		},
		// the same as above, but jumps are avoided (conditional moves are used)
		[](item_t a, item_t bdepth) -> item_t {
			if (bdepth - a <= 1) {
				a = bdepth - 3;
			} 
			if (bdepth - a > 10) {
				a = bdepth - 10;
			}
			return std::max<decltype(a)>(a, 3);
		},
		[](item_t a, item_t bdepth) -> item_t {
			a = std::min<decltype(a)>(a, bdepth - 3 + (item_t)(bdepth - a == 2));
			a = std::max<decltype(a)>(a, bdepth - 10);
			return std::max<decltype(a)>(a, 3);
		},
		// UB when defining comparison in a way below
		// [](item_t a, item_t bdepth) -> item_t {
		// 	a = std::min<decltype(a)>(a, bdepth - 3,
		// 		[](const item_t& f, const item_t& s) { return f < s + 2; });
		// 	a = std::max<decltype(a)>(a, bdepth - 10);
		// 	return std::max<decltype(a)>(a, 3);
		// }
	};

	constexpr auto fsize = sizeof(testFunctions) / sizeof(*testFunctions);
	std::array<item_t, fsize> buffer = {};
	std::array<size_t, fsize> statistics = { 0 };

	typedef std::tuple<item_t, item_t, item_t> setvalue_t;
	typedef std::tuple<std::pair<item_t, item_t>, decltype(buffer)> set_t;

	std::vector<set_t> diffs;
	for (item_t bdepthDc = 0; bdepthDc <= 32; ++bdepthDc) {
		for (item_t bdepthAc = bdepthDc; bdepthAc <= 32; ++bdepthAc) {
			for (ptrdiff_t i = 0; i < fsize; ++i) {
				buffer[i] = testFunctions[i](((bdepthAc >> 1) + 1), bdepthDc);
			}
			for (ptrdiff_t i = 0; i < fsize; ++i) {
				if (buffer[0] != buffer[i]) {
					++statistics[i];
				}
			}
			for (ptrdiff_t i = 1; i < fsize; ++i) {
				if (buffer[i] != buffer[i - 1]) {
					diffs.push_back(std::make_tuple(
						std::make_pair(bdepthDc, bdepthAc),
						buffer));
					break;
				}
			}
		}
	}

	for (auto item : diffs) {
		auto item_value = std::get<1>(item);
		std::cout << "[" << std::get<0>(item).first << ", " << std::get<0>(item).second << "] : ";

		std::apply(
			[](auto const& ... args) {
				std::cout << "(";
				unroll<true, UnrollFoldType::right>::apply(
					[]<size_t N>(auto param) -> void { std::cout << param; },
					[]() -> void { std::cout << ", "; }, 
					args...);
				std::cout << ") ";
			}, std::get<1>(item));
		std::cout << std::endl;
		
		// runtime implementation
		// auto & bufref = std::get<1>(item);
		// if (bufref.size() > 0) {
		// 	ptrdiff_t i = 0;
		// 	std::cout << "(";
		// 	for (; i < bufref.size() - 1; ++i) {
		// 		std::cout << bufref[i] << ", ";
		// 	}
		// 	std::cout << bufref[i] << ") ";
		// }
	}
	std::cout << "Match statistics: ";
	std::apply(
		[](auto const& ... args) {
			std::cout << "{ ";
			unroll<true, UnrollFoldType::right>::apply(
				[]<size_t>(auto param) -> void { std::cout << param; },
				[]() -> void {std::cout << ", "; },
				args...);
			std::cout << " } ";
		}, statistics);
	std::cout << std::endl;

	EXPECT_EQ(diffs.size(), 0);
}

#include "dwt/dwt.hpp"
#include "dwt/segment_assembly.hpp"
#include "test_utils.tpp"

#include "io/ccsds_protocol.hpp"

TEST(compile, io) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	shifts_t subband_shifts{ 0 };

	ForwardWaveletTransformer<item_t> dwt;
	dwt.get_scale().set_shifts(subband_shifts);
	auto i_coeffs = dwt.apply(input);

	SegmentAssembler<item_t> precoder;
	precoder.set_shifts(subband_shifts);
	auto output = precoder.apply(std::move(i_coeffs));

	ccsds_protocol protocol(*(output[0]), 0);
}