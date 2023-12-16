#include "gtest/gtest.h"
#include "../WaveletTransform/DWT.tpp"
#include "../WaveletTransform/dwtcore.tpp"
#include <random>

TEST(dwtcorei, DwtCoreI) {
	typedef long long item_t;
	item_t* pinput = new item_t[512]{ 0 };
	item_t* phout = new item_t[256]{ 0 };
	item_t* plout = new item_t[256]{ 0 };
	item_t* poutput = new item_t[512]{ 0 };

	constexpr size_t sampleNum = 512 - 128;
	constexpr size_t coeffNum = sampleNum / 2;
	constexpr double pi = 3.141592653589793238;
	constexpr double stride = pi * 2 / sampleNum;
	constexpr size_t i_row_start = 64;
	constexpr size_t i_coef_start = i_row_start / 2;
	constexpr size_t mirror_num = 5;

	item_t* input = (item_t*)(&pinput[i_row_start]);
	item_t* hout = (item_t*)(&phout[i_coef_start]);
	item_t* lout = (item_t*)(&plout[i_coef_start]);
	item_t* output = (item_t*)(&poutput[i_row_start]);

	double argument = 0;
	std::mt19937_64 generator(1067);
	std::uniform_real_distribution<> realdis(-5.0f, 5.0f);
	for (size_t i = 0; i < sampleNum; i++) {
		input[i] = (item_t)((realdis(generator) + cos(argument * 3) + sin(argument * 2)) * 1024 * 16);
		argument += stride;
	}

	dwtcore<item_t> mycore;
	mycore.extfwd(input);
	mycore.rextfwd(input + sampleNum);
	constexpr size_t segmentNum = 2;
	constexpr size_t sps = sampleNum / segmentNum;
	constexpr size_t cps = sps / 2;
	for (size_t i = 0; i < segmentNum; ++i) {
		mycore.fwd(input + i * sps, hout + i * cps, lout + i * cps, cps);
	}

	mycore.corrlfwd(input, hout, lout);
	mycore.exthbwd(hout);
	mycore.rexthbwd(hout + coeffNum);
	mycore.extlbwd(lout);
	mycore.rextlbwd(lout + coeffNum);

	mycore.bwd(hout, lout, output, coeffNum);

	for (size_t i = 0; i < sampleNum; ++i) {
		EXPECT_EQ((item_t)input[i], (item_t)output[i]) << " at index " << i;
	}
	delete[] poutput;
	delete[] plout;
	delete[] phout;
	delete[] pinput;
}

template <typename T>
bitmap<T> generateNoisyBitmap(size_t width, size_t height, size_t offset, 
		size_t seed = 1067, double phShift = 0.173f) {
	bitmap<T> input(width, height, offset);
	constexpr double pi = 3.141592653589793238;
	constexpr double stride = pi / (1 << 10);

	std::mt19937_64 generator(seed);
	std::uniform_real_distribution<> realdis(-5.0f, 5.0f);
	for (size_t i = 0; i < height; ++i) {
		double argument = phShift * i;
		for (size_t j = 0; j < width; ++j) {
			input[i][j] = (T)((realdis(generator) + cos(argument * 3) + sin(argument * 2)) * 1024 * 16);
			argument += stride;
		}
	}
	return input;
}

TEST(dwti, 2dSmoke) {
	typedef long long item_t;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input(props.width, props.height, 32);
	constexpr double pi = 3.141592653589793238;
	constexpr double stride = pi / (1 << 10);

	std::mt19937_64 generator(1067);
	std::uniform_real_distribution<> realdis(-5.0f, 5.0f);
	for (size_t i = 0; i < props.height; ++i) {
		double argument = 0.173 * i;
		for (size_t j = 0; j < props.width; ++j) {
			input[i][j] = (item_t)((realdis(generator) + cos(argument * 3) + sin(argument * 2)) * 1024 * 16);
			argument += stride;
		}
	}

	ForwardWaveletTransformer<item_t> dwt(props);
	auto m_blocks = dwt.apply(input);
	auto i_coeffs = dwt._getBuffers();

	img_pos reverser_props = props;
	reverser_props.width /= 8;
	reverser_props.height /= 8;
	BackwardWaveletTransformer<item_t> reverser(reverser_props);
	bitmap<item_t> output = reverser.apply(m_blocks);
	auto o_coeffs = reverser._getBuffers();

	// size_t verify_index = 8;
	for (ptrdiff_t verify_index = i_coeffs.size() - 1; verify_index >= 0 ; --verify_index) {
		img_meta temp = o_coeffs[verify_index].getImgMeta();
		size_t i = 0;
		size_t j = 0;
		try {
			for (; i < temp.height; ++i) {
				for (; j < temp.width; ++j) {
					if (o_coeffs[verify_index][i][j] != i_coeffs[verify_index][j][i]) {
						throw "NEQ!";
					}
				}
			}
		}
		catch (...) {
			EXPECT_TRUE(false) << "plane: " << verify_index << "at index [" << i << "][" << j << "]";
		}
	}

	img_meta input_props = input.getImgMeta();
	img_meta output_props = output.getImgMeta();
	ASSERT_EQ(input_props.width, output_props.width);
	ASSERT_EQ(input_props.height, output_props.height);

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}

TEST(tbitmap, bitmapMemorySmoke) {
	typedef long long item_t;
	constexpr size_t allignment = 16;
	constexpr size_t testIterations = 1 << 7;

	size_t seed = 1067;
	std::mt19937_64 generator(seed);
	std::uniform_real_distribution<> realdis(-5.0f, 5.0f);
	size_t img_dim_mask = ~(-1 << 10);
	size_t img_offset_mask = ~(-1 << 6);
	for (size_t i = 0; i < testIterations; ++i) {
		img_pos props;

		props.width = generator() & img_dim_mask;
		props.height = generator() & img_dim_mask;
		size_t offset = generator() & img_offset_mask;
		seed = ((seed + (props.width ^ props.height)) ^ generator()) ^ img_dim_mask;
		bitmap<item_t> first = generateNoisyBitmap<item_t>(props.width, props.height, offset, seed);

		props.width = generator() & img_dim_mask;
		props.height = generator() & img_dim_mask;
		offset = generator() & img_offset_mask;
		seed = ((seed + (props.width ^ props.height)) ^ generator()) ^ img_dim_mask;
		bitmap<item_t> second = generateNoisyBitmap<item_t>(props.width, props.height, offset, seed);

		img_meta second_meta = second.getImgMeta();
		size_t slice_x = generator() % second_meta.width;
		size_t slice_y = generator() % second_meta.height;
		size_t slice_len = generator() % (second_meta.width * (second_meta.height - slice_y) - slice_x);
		std::vector<item_t> first_slice(slice_len + 3 * allignment);
		std::vector<item_t> second_slice(slice_len + 3 * allignment);
		std::vector<item_t> control_slice(slice_len + 3 * allignment);

		// get linear slice second
		// first <-(move) second
		// get same linear slice
		// second <-(copy) first
		// compare slices
		// compare second to slice

		constexpr size_t pallignment = allignment * sizeof(item_t);
		item_t* second_slice_ptr = (item_t*)((((size_t)(second_slice.data())) + (2 * pallignment - 1)) & (~(pallignment - 1)));
		second.linear(second_slice_ptr, slice_len, slice_x, slice_y);
		first = std::move(second);
		item_t* first_slice_ptr = (item_t*)((((size_t)(first_slice.data())) + (2 * pallignment - 1)) & (~(pallignment - 1)));
		first.linear(first_slice_ptr, slice_len, slice_x, slice_y);
		second = std::forward<bitmap<item_t>&>(first);
		item_t* control_slice_ptr = (item_t*)((((size_t)(control_slice.data())) + (2 * pallignment - 1)) & (~(pallignment - 1)));
		second.linear(control_slice_ptr, slice_len, slice_x, slice_y);

		for (size_t j = 0; j < slice_len; ++j) {
			EXPECT_EQ(first_slice_ptr[j], control_slice_ptr[j]) << " at index [" << j << "], iteration " << i << "";
			EXPECT_EQ(second_slice_ptr[j], control_slice_ptr[j]) << " at index [" << j << "], iteration " << i << "";
		}

		for (size_t ii = 0; ii < props.height; ++ii) {
			for (size_t jj = 0; jj < props.width; ++jj) {
				EXPECT_EQ(first[ii][jj], second[ii][jj]) << " at index [" << ii << "][" << jj << "]";
			}
		}
	}
}

#include "../WaveletTransform/SegmentPreCoder.tpp"
TEST(segments, PrecoderSmoke) {
	typedef long long item_t;
	constexpr size_t allignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	ForwardWaveletTransformer<item_t> dwt(props);
	auto m_blocks = dwt.apply(input);
	auto i_coeffs = dwt._getBuffers();
	std::vector<bitmap<item_t>> coeffs_v(i_coeffs.cbegin(), i_coeffs.cend());
	SegmentPreCoder<item_t> precoder(coeffs_v);
	auto output = precoder.apply();

	for (size_t i = 0; i < output.size(); ++i) {
		EXPECT_LE(output[i].q, output[i].bdepthDc) << " q > depth DC at index [" << i << "]";
		EXPECT_LE(output[i].q, output[i].bdepthDc) << " q > depth DC at index [" << i << "]";
	}
}

TEST(segments, PrecoderRound) {
	typedef long long item_t;
	constexpr size_t allignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	ForwardWaveletTransformer<item_t> dwt(props);
	auto m_blocks = dwt.apply(input);
	auto i_coeffs = dwt._getBuffers();
	std::vector<bitmap<item_t>> coeffs_v(i_coeffs.cbegin(), i_coeffs.cend());
	SegmentPreCoder<item_t> precoder(coeffs_v);
	auto coded = precoder.apply();
	SegmentPostDecoder<item_t> postdecoder(coded);
	auto decoded = postdecoder.apply(props.width);
	bitmap<item_t> output;

	size_t out_row_offset = 0;
	for (auto& block: decoded) {
		img_meta meta = block[0].getImgMeta();
		img_pos fragment_props { 0 };
		fragment_props.width = meta.width;
		fragment_props.height = meta.height;
		BackwardWaveletTransformer<item_t> bdwt(fragment_props);
		bdwt._setPayloadBuffers(block);
		auto out_fragment = bdwt.apply();
		size_t out_fragment_height; // = relu(out_fragment.getImgMeta().height - 16);
		if (&block != &(decoded.back())) {
			out_fragment_height = relu(out_fragment.getImgMeta().height - 16);
		} else {
			out_fragment_height = out_fragment.getImgMeta().height;
		}
		output.resize(props.width, out_row_offset + out_fragment_height);
		for (size_t i = 0; i < out_fragment_height; ++i) {
			output[out_row_offset + i].assign(out_fragment[i]);
		}
		out_row_offset += out_fragment_height;
	}


	img_meta input_props = input.getImgMeta();
	img_meta output_props = output.getImgMeta();
	ASSERT_EQ(input_props.width, output_props.width);
	ASSERT_EQ(input_props.height, output_props.height);

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}

#include <functional>
#include "../common/utils.h"

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