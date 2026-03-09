#include "gtest/gtest.h"

#include "test_utils.tpp"

#include "dwt/dwt.hpp"

TEST(dwti, minimalImageRound) {
	typedef int64_t item_t;
	img_pos props;
	props.width = 17;
	props.height = 17;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	ForwardWaveletTransformer<item_t> dwt;
	auto subbands = dwt.apply(input);

	BackwardWaveletTransformer<item_t> reverser;
	reverser.get_scale().set_shifts(dwt.get_scale().get_shifts());
	reverser.set_skip_dc_scaling(false);
	bitmap<item_t> output = reverser.apply(subbands);

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}

TEST(dwti, smallImage128Round) {
	typedef int64_t item_t;
	img_pos props;
	props.width = 128;
	props.height = 128;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	ForwardWaveletTransformer<item_t> dwt;
	auto subbands = dwt.apply(input);

	BackwardWaveletTransformer<item_t> reverser;
	reverser.get_scale().set_shifts(dwt.get_scale().get_shifts());
	reverser.set_skip_dc_scaling(false);
	bitmap<item_t> output = reverser.apply(subbands);

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}

TEST(dwti, smallImage256Round) {
	typedef int64_t item_t;
	img_pos props;
	props.width = 256;
	props.height = 256;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	ForwardWaveletTransformer<item_t> dwt;
	auto subbands = dwt.apply(input);

	BackwardWaveletTransformer<item_t> reverser;
	reverser.get_scale().set_shifts(dwt.get_scale().get_shifts());
	reverser.set_skip_dc_scaling(false);
	bitmap<item_t> output = reverser.apply(subbands);

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}
