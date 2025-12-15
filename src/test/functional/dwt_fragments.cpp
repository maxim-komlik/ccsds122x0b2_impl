#include "gtest/gtest.h"

#include "../test_utils.tpp"

#include <vector>

#include "dwt/dwt.hpp"

TEST(dwti, framedTransform) {
	typedef long long item_t;
	img_pos props;
	props.width = (1 << 10) - 7;
	props.height = (1 << 11) - 7;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	img_pos frame;
	frame.width = 624;
	frame.height = 128;
	frame.depth = 1;
	frame.x_step = frame.width;
	frame.y_step = frame.height;
	frame.x_stride = input.get_meta().stride;
	frame.x = 0;
	frame.y = 0;

	ForwardWaveletTransformer<item_t> dwt;
	dwt.preprocess_image(input);

	std::vector<subbands_t<item_t>> subband_collection;
	constexpr size_t padding_size = 8;
	size_t img_width_padded = (props.width + padding_size - 1) & (~(padding_size - 1));
	while ((frame.x < props.width) & (frame.y < props.height)) {
		subband_collection.push_back(dwt.apply(input, frame));
		frame.x += frame.x_step;
		if (frame.x >= img_width_padded) {
			frame.x -= img_width_padded;
			frame.y += frame.y_step;
		}
	};

	auto single_shot_subbands = dwt.apply(input);

	auto make_subbands = [](size_t img_width, size_t img_height) -> subbands_t<item_t> {
		constexpr size_t src_padding_size = 8;
		constexpr size_t family_num = 3;
		constexpr size_t items_per_family = 3;
		img_width = (img_width + src_padding_size - 1) & (~(src_padding_size - 1));
		img_height = (img_height + src_padding_size - 1) & (~(src_padding_size - 1));

		subbands_t<item_t> result;
		for (ptrdiff_t i = 0; i < family_num; ++i) {
			size_t shift_amount = (family_num - i);
			for (ptrdiff_t j = 0; j < items_per_family; ++j) {
				result[i * items_per_family + j + 1].resize(img_width >> shift_amount, img_height >> shift_amount);
			}
		}
		result[0].resize(img_width >> 3, img_height >> 3);
		return result;
	};

	subbands_t<item_t> merged_subbands = make_subbands(props.width, props.height);
	frame.x = 0;
	frame.y = 0;
	props.width = (props.width + padding_size - 1) & (~(padding_size - 1));
	props.height = (props.height + padding_size - 1) & (~(padding_size - 1));
	for (auto& item : subband_collection) {
		frame.width = item[0].get_meta().width << 3;
		frame.height = item[0].get_meta().height << 3;
		// ignore height?
		auto factor_value_for_index = [](ptrdiff_t index) -> ptrdiff_t {
			if (index < 4) {
				return 3;
			}
			if (index < 7) {
				return 2;
			}
			return 1;
		};

		for (ptrdiff_t i = 0; i < item.size(); ++i) {
			img_pos aux_frame = item[i].single_frame_params();
			ptrdiff_t s_x = frame.x >> factor_value_for_index(i);
			ptrdiff_t s_y = frame.y >> factor_value_for_index(i);

			size_t remaining_width = aux_frame.width;
			while (aux_frame.x < item[i].get_meta().width) {
				aux_frame.width = std::min(merged_subbands[i].get_meta().width - s_x, remaining_width);
				remaining_width -= aux_frame.width;

				img_pos m_frame = aux_frame;
				m_frame.x = s_x;
				m_frame.y = s_y;

				merged_subbands[i].slice(m_frame).assign(item[i].slice(aux_frame));

				s_x += m_frame.width;
				if (s_x >= merged_subbands[i].get_meta().width) {
					s_x = 0; 
					s_y += m_frame.height;
				}

				aux_frame.x += aux_frame.width;
			}
		}

		frame.x += frame.width;
		if (frame.x >= props.width) {
			frame.x -= props.width;
			frame.y += frame.height;
		}
	}

	BackwardWaveletTransformer<item_t> reverser;
	reverser.get_scale().set_shifts(dwt.get_scale().get_shifts());
	reverser.set_skip_dc_scaling(false);
	bitmap<item_t> output = reverser.apply(merged_subbands);

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();

	for (size_t i = 0; i < input_props.height; ++i) {
		for (size_t j = 0; j < input_props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}

TEST(dwti, narrowFrameTransform) {
	typedef long long item_t;
	img_pos props;
	props.width = (1 << 10) - 7;
	props.height = (1 << 9) - 7;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	img_pos frame;
	frame.width = 624;
	frame.height = 8;
	frame.depth = 1;
	frame.x_step = frame.width;
	frame.y_step = frame.height;
	frame.x_stride = input.get_meta().stride;
	frame.x = 0;
	frame.y = 0;

	ForwardWaveletTransformer<item_t> dwt;
	dwt.preprocess_image(input);

	std::vector<subbands_t<item_t>> subband_collection;
	constexpr size_t padding_size = 8;
	size_t img_width_padded = (props.width + padding_size - 1) & (~(padding_size - 1));
	while ((frame.x < props.width) & (frame.y < props.height)) {
		subband_collection.push_back(dwt.apply(input, frame));
		frame.x += frame.x_step;
		if (frame.x >= img_width_padded) {
			frame.x -= img_width_padded;
			frame.y += frame.y_step;
		}
	};

	auto single_shot_subbands = dwt.apply(input);

	auto make_subbands = [](size_t img_width, size_t img_height) -> subbands_t<item_t> {
		constexpr size_t src_padding_size = 8;
		constexpr size_t family_num = 3;
		constexpr size_t items_per_family = 3;
		img_width = (img_width + src_padding_size - 1) & (~(src_padding_size - 1));
		img_height = (img_height + src_padding_size - 1) & (~(src_padding_size - 1));

		subbands_t<item_t> result;
		for (ptrdiff_t i = 0; i < family_num; ++i) {
			size_t shift_amount = (family_num - i);
			for (ptrdiff_t j = 0; j < items_per_family; ++j) {
				result[i * items_per_family + j + 1].resize(img_width >> shift_amount, img_height >> shift_amount);
			}
		}
		result[0].resize(img_width >> 3, img_height >> 3);
		return result;
	};

	subbands_t<item_t> merged_subbands = make_subbands(props.width, props.height);
	frame.x = 0;
	frame.y = 0;
	props.width = (props.width + padding_size - 1) & (~(padding_size - 1));
	props.height = (props.height + padding_size - 1) & (~(padding_size - 1));
	for (auto& item : subband_collection) {
		frame.width = item[0].get_meta().width << 3;
		frame.height = item[0].get_meta().height << 3;
		// ignore height?
		auto factor_value_for_index = [](ptrdiff_t index) -> ptrdiff_t {
			if (index < 4) {
				return 3;
			}
			if (index < 7) {
				return 2;
			}
			return 1;
			};

		for (ptrdiff_t i = 0; i < item.size(); ++i) {
			img_pos aux_frame = item[i].single_frame_params();
			ptrdiff_t s_x = frame.x >> factor_value_for_index(i);
			ptrdiff_t s_y = frame.y >> factor_value_for_index(i);

			size_t remaining_width = aux_frame.width;
			while (aux_frame.x < item[i].get_meta().width) {
				aux_frame.width = std::min(merged_subbands[i].get_meta().width - s_x, remaining_width);
				remaining_width -= aux_frame.width;

				img_pos m_frame = aux_frame;
				m_frame.x = s_x;
				m_frame.y = s_y;

				merged_subbands[i].slice(m_frame).assign(item[i].slice(aux_frame));

				s_x += m_frame.width;
				if (s_x >= merged_subbands[i].get_meta().width) {
					s_x = 0; 
					s_y += m_frame.height;
				}

				aux_frame.x += aux_frame.width;
			}
		}

		frame.x += frame.width;
		if (frame.x >= props.width) {
			frame.x -= props.width;
			frame.y += frame.height;
		}
	}

	BackwardWaveletTransformer<item_t> reverser;
	reverser.get_scale().set_shifts(dwt.get_scale().get_shifts());
	reverser.set_skip_dc_scaling(false);
	bitmap<item_t> output = reverser.apply(merged_subbands);

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();

	for (size_t i = 0; i < input_props.height; ++i) {
		for (size_t j = 0; j < input_props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}