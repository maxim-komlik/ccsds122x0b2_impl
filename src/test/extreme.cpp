#include "gtest/gtest.h"

#include "dwt/dwt.hpp"
#include "dwt/segment_assembly.hpp"
#include "bpe/bpe.tpp"

#include "test_utils.tpp"

TEST(sparse, dwtiRound) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);
	input >>= 15;

	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
	// shifts_t subband_shifts{ 3, 3, 2, 1, 3, 2, 1, 3, 2, 1 };
	shifts_t subband_shifts{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	ForwardWaveletTransformer<item_t> dwt;
	dwt.get_scale().set_shifts(subband_shifts);
	auto i_coeffs = dwt.apply(input);

	BackwardWaveletTransformer<item_t> bdwt;
	bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
	bdwt.set_skip_dc_scaling(false);
	bitmap<item_t> output = bdwt.apply(i_coeffs);

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();
	ASSERT_EQ(input_props.width, output_props.width);
	ASSERT_EQ(input_props.height, output_props.height);

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}

TEST(sparse, segmentsRound) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);
	input >>= 15;

	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
	shifts_t subband_shifts{ 3, 3, 2, 1, 3, 2, 1, 3, 2, 1 };
	// shifts_t subband_shifts{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	ForwardWaveletTransformer<item_t> dwt;
	dwt.get_scale().set_shifts(subband_shifts);
	auto i_coeffs = dwt.apply(input);

	SegmentAssembler<item_t> precoder;
	precoder.set_shifts(dwt.get_scale().get_shifts());
	auto precoded = precoder.apply(std::move(i_coeffs));

	bitmap<item_t> output;

	{
		SegmentDisassembler<item_t> postdecoder;
		postdecoder.set_image_width(props.width);
		auto decoded = postdecoder.apply(std::move(precoded));

		size_t out_row_offset = 0;
		for (auto& block : decoded) {
			img_meta meta = block[0].get_meta();
			img_pos fragment_props{ 0 };
			fragment_props.width = meta.width;
			fragment_props.height = meta.height;
			BackwardWaveletTransformer<item_t> bdwt;
			bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
			bdwt.set_skip_dc_scaling(false);
			auto out_fragment = bdwt.apply(block);
			size_t out_fragment_height; // = relu(out_fragment.get_meta().height - 16);
			if (&block != &(decoded.back())) {
				out_fragment_height = relu(out_fragment.get_meta().height - 16);
			}
			else {
				out_fragment_height = out_fragment.get_meta().height;
			}
			output.resize(props.width, out_row_offset + out_fragment_height);
			for (size_t i = 0; i < out_fragment_height; ++i) {
				output[out_row_offset + i].assign(out_fragment[i]);
			}
			out_row_offset += out_fragment_height;
		}
	}

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();
	ASSERT_EQ(input_props.width, output_props.width);
	ASSERT_EQ(input_props.height, output_props.height);

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}

TEST(sparse, bpeRound) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);
	input >>= 15;

	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
	shifts_t subband_shifts{ 2, 3, 2, 1, 3, 2, 1, 3, 2, 1 };
	// shifts_t subband_shifts{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	ForwardWaveletTransformer<item_t> dwt;
	dwt.get_scale().set_shifts(subband_shifts);
	auto i_coeffs = dwt.apply(input);

	SegmentAssembler<item_t> precoder;
	precoder.set_shifts(dwt.get_scale().get_shifts());
	auto precoded = precoder.apply(std::move(i_coeffs));

	auto bpe_input_segment = *(precoded[0]);
	BitPlaneEncoder<decltype(bpe_input_segment)::type> bpeencoder;
	bpeencoder.set_use_heuristic_DC(false);
	bpeencoder.set_use_heuristic_bdepthAc(false);
	BitPlaneDecoder<decltype(bpe_input_segment)::type> bpedecoder;

	std::vector<uint64_t> compressed;
	// constexpr size_t stage_limit = 0x0937; // stage 2 debug
	// constexpr size_t stage_limit = 0x0a4a; // stage 3 debug
	constexpr size_t stage_limit = (0x01 << 24) - 1;

	{
		auto bpe_debug_output_buffer_callback = [&compressed](uint64_t item) -> void {
			compressed.push_back(item);
		};
		obitwrapper<uint64_t> boutput(bpe_debug_output_buffer_callback, stage_limit << 3);

		try {
			bpeencoder.encode(bpe_input_segment, boutput);
		}
		catch (const ccsds::bpe::byte_limit_exception& ex) {
			std::cout << "Byte limit reached when encoding segment 0, output size: "
				<< compressed.size() << std::endl;
		}
		boutput.flush();
	}
	bitmap<item_t> output;

	{
		segment<item_t> backward_input;
		backward_input.size = bpe_input_segment.size;
		backward_input.bdepthAc = bpe_input_segment.bdepthAc;
		backward_input.bdepthDc = bpe_input_segment.bdepthDc;
		backward_input.bit_shifts = bpe_input_segment.bit_shifts;

		// determines T type, T should be the smallest type with bdepth(T) is not less than bdepth_pixel
		size_t bdepth_pixel;
		// used by segmentPostDecoder
		size_t image_width;
		// determines buffer type of obitwrapper (or other encoding structure used by other implementation)
		// and indicates if additional padding should be appended to the end of input collection to align 
		// to the ibitwrapper buffer type word boundary
		size_t codeword_length;
		// SegByteLimit is a parameter for ibitwrapper/obitwrapper to terminate early.
		size_t input_size_bytes; // aka SegByteLimit

		// size_t input_size; // aka Header part 3 S
		// std::array<size_t, 10> bit_shifts{ 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };
		// size_t bdepthAc;
		// size_t bdepthDc;

		{
			size_t compressed_index = 0;
			auto bpe_debug_input_buffer_callback = [&compressed, &compressed_index]() -> uint64_t {
				// static auto iter = compressed.begin();
				return compressed[compressed_index++];
				};
			ibitwrapper<uint64_t> binput(bpe_debug_input_buffer_callback, stage_limit << 3);

			try {
				bpedecoder.decode(backward_input, binput);
			}
			catch (const ccsds::bpe::byte_limit_exception& ex) {
				std::cout << "Byte limit reached when decoding segment 0, decoded output size: "
					<< compressed_index << std::endl;
			}
		}

		for (ptrdiff_t i = 0; i < backward_input.size; ++i) {
			for (ptrdiff_t j = 1; j < 64; ++j) { // TODO: items per block
				EXPECT_EQ(backward_input.data[i].content[j], precoded[0]->data[i].content[j])
					<< " at segment [0], block [" << i << "], coeff [" << j << "]";
			}
		}

		*(precoded[0]) = backward_input;
		SegmentDisassembler<item_t> postdecoder;
		postdecoder.set_image_width(props.width);
		auto decoded = postdecoder.apply(std::move(precoded));

		size_t out_row_offset = 0;
		for (auto& block : decoded) {
			img_meta meta = block[0].get_meta();
			img_pos fragment_props{ 0 };
			fragment_props.width = meta.width;
			fragment_props.height = meta.height;
			BackwardWaveletTransformer<item_t> bdwt;
			bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
			auto out_fragment = bdwt.apply(block);
			size_t out_fragment_height; // = relu(out_fragment.get_meta().height - 16);
			if (&block != &(decoded.back())) {
				out_fragment_height = relu(out_fragment.get_meta().height - 16);
			}
			else {
				out_fragment_height = out_fragment.get_meta().height;
			}
			output.resize(props.width, out_row_offset + out_fragment_height);
			for (size_t i = 0; i < out_fragment_height; ++i) {
				output[out_row_offset + i].assign(out_fragment[i]);
			}
			out_row_offset += out_fragment_height;
		}
	}

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();
	ASSERT_EQ(input_props.width, output_props.width);
	ASSERT_EQ(input_props.height, output_props.height);

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}

TEST(sparse, bpeRound_002) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);
	input >>= 18;

	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
	// shifts_t subband_shifts{ 2, 3, 2, 1, 3, 2, 1, 3, 2, 1 };
	shifts_t subband_shifts{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	ForwardWaveletTransformer<item_t> dwt;
	dwt.get_scale().set_shifts(subband_shifts);
	auto i_coeffs = dwt.apply(input);

	SegmentAssembler<item_t> precoder;
	precoder.set_shifts(dwt.get_scale().get_shifts());
	auto precoded = precoder.apply(std::move(i_coeffs));

	auto bpe_input_segment = *(precoded[0]);
	BitPlaneEncoder<decltype(bpe_input_segment)::type> bpeencoder;
	bpeencoder.set_use_heuristic_DC(false);
	bpeencoder.set_use_heuristic_bdepthAc(false);
	BitPlaneDecoder<decltype(bpe_input_segment)::type> bpedecoder;

	std::vector<uint64_t> compressed;
	// constexpr size_t stage_limit = 0x0937; // stage 2 debug
	// constexpr size_t stage_limit = 0x0a4a; // stage 3 debug
	constexpr size_t stage_limit = (0x01 << 24) - 1;

	{
		auto bpe_debug_output_buffer_callback = [&compressed](uint64_t item) -> void {
			compressed.push_back(item);
		};
		obitwrapper<uint64_t> boutput(bpe_debug_output_buffer_callback, stage_limit << 3);

		try {
			bpeencoder.encode(bpe_input_segment, boutput);
		}
		catch (const ccsds::bpe::byte_limit_exception& ex) {
			std::cout << "Byte limit reached when encoding segment 0, output size: "
				<< compressed.size() << std::endl;
		}
		boutput.flush();
	}
	bitmap<item_t> output;

	{
		segment<item_t> backward_input;
		backward_input.size = bpe_input_segment.size;
		backward_input.bdepthAc = bpe_input_segment.bdepthAc;
		backward_input.bdepthDc = bpe_input_segment.bdepthDc;
		backward_input.bit_shifts = bpe_input_segment.bit_shifts;

		// determines T type, T should be the smallest type with bdepth(T) is not less than bdepth_pixel
		size_t bdepth_pixel;
		// used by segmentPostDecoder
		size_t image_width;
		// determines buffer type of obitwrapper (or other encoding structure used by other implementation)
		// and indicates if additional padding should be appended to the end of input collection to align 
		// to the ibitwrapper buffer type word boundary
		size_t codeword_length;
		// SegByteLimit is a parameter for ibitwrapper/obitwrapper to terminate early.
		size_t input_size_bytes; // aka SegByteLimit

		// size_t input_size; // aka Header part 3 S
		// std::array<size_t, 10> bit_shifts{ 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };
		// size_t bdepthAc;
		// size_t bdepthDc;

		{
			size_t compressed_index = 0;
			auto bpe_debug_input_buffer_callback = [&compressed, &compressed_index]() -> uint64_t {
				// static auto iter = compressed.begin();
				return compressed[compressed_index++];
				};
			ibitwrapper<uint64_t> binput(bpe_debug_input_buffer_callback, stage_limit << 3);

			try {
				bpedecoder.decode(backward_input, binput);
			}
			catch (const ccsds::bpe::byte_limit_exception& ex) {
				std::cout << "Byte limit reached when decoding segment 0, decoded output size: "
					<< compressed_index << std::endl;
			}
		}

		for (ptrdiff_t i = 0; i < backward_input.size; ++i) {
			for (ptrdiff_t j = 1; j < 64; ++j) { // TODO: items per block
				EXPECT_EQ(backward_input.data[i].content[j], precoded[0]->data[i].content[j])
					<< " at segment [0], block [" << i << "], coeff [" << j << "]";
			}
		}

		*(precoded[0]) = backward_input;
		SegmentDisassembler<item_t> postdecoder;
		postdecoder.set_image_width(props.width);
		auto decoded = postdecoder.apply(std::move(precoded));

		size_t out_row_offset = 0;
		for (auto& block : decoded) {
			img_meta meta = block[0].get_meta();
			img_pos fragment_props{ 0 };
			fragment_props.width = meta.width;
			fragment_props.height = meta.height;
			BackwardWaveletTransformer<item_t> bdwt;
			bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
			auto out_fragment = bdwt.apply(block);
			size_t out_fragment_height; // = relu(out_fragment.get_meta().height - 16);
			if (&block != &(decoded.back())) {
				out_fragment_height = relu(out_fragment.get_meta().height - 16);
			}
			else {
				out_fragment_height = out_fragment.get_meta().height;
			}
			output.resize(props.width, out_row_offset + out_fragment_height);
			for (size_t i = 0; i < out_fragment_height; ++i) {
				output[out_row_offset + i].assign(out_fragment[i]);
			}
			out_row_offset += out_fragment_height;
		}
	}

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();
	ASSERT_EQ(input_props.width, output_props.width);
	ASSERT_EQ(input_props.height, output_props.height);

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}

TEST(sparse, bpeRound_003) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);
	input >>= 24;

	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
	// shifts_t subband_shifts{ 2, 3, 2, 1, 3, 2, 1, 3, 2, 1 };
	shifts_t subband_shifts{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	ForwardWaveletTransformer<item_t> dwt;
	dwt.get_scale().set_shifts(subband_shifts);
	auto i_coeffs = dwt.apply(input);

	SegmentAssembler<item_t> precoder;
	precoder.set_shifts(dwt.get_scale().get_shifts());
	auto precoded = precoder.apply(std::move(i_coeffs));

	auto bpe_input_segment = *(precoded[0]);
	BitPlaneEncoder<decltype(bpe_input_segment)::type> bpeencoder;
	bpeencoder.set_use_heuristic_DC(false);
	bpeencoder.set_use_heuristic_bdepthAc(false);
	BitPlaneDecoder<decltype(bpe_input_segment)::type> bpedecoder;

	std::vector<uint64_t> compressed;
	// constexpr size_t stage_limit = 0x0937; // stage 2 debug
	// constexpr size_t stage_limit = 0x0a4a; // stage 3 debug
	constexpr size_t stage_limit = (0x01 << 24) - 1;

	{
		auto bpe_debug_output_buffer_callback = [&compressed](uint64_t item) -> void {
			compressed.push_back(item);
		};
		obitwrapper<uint64_t> boutput(bpe_debug_output_buffer_callback, stage_limit << 3);

		try {
			bpeencoder.encode(bpe_input_segment, boutput);
		}
		catch (const ccsds::bpe::byte_limit_exception& ex) {
			std::cout << "Byte limit reached when encoding segment 0, output size: "
				<< compressed.size() << std::endl;
		}
		boutput.flush();
	}
	bitmap<item_t> output;

	{
		segment<item_t> backward_input;
		backward_input.size = bpe_input_segment.size;
		backward_input.bdepthAc = bpe_input_segment.bdepthAc;
		backward_input.bdepthDc = bpe_input_segment.bdepthDc;
		backward_input.bit_shifts = bpe_input_segment.bit_shifts;

		// determines T type, T should be the smallest type with bdepth(T) is not less than bdepth_pixel
		size_t bdepth_pixel;
		// used by segmentPostDecoder
		size_t image_width;
		// determines buffer type of obitwrapper (or other encoding structure used by other implementation)
		// and indicates if additional padding should be appended to the end of input collection to align 
		// to the ibitwrapper buffer type word boundary
		size_t codeword_length;
		// SegByteLimit is a parameter for ibitwrapper/obitwrapper to terminate early.
		size_t input_size_bytes; // aka SegByteLimit

		// size_t input_size; // aka Header part 3 S
		// std::array<size_t, 10> bit_shifts{ 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };
		// size_t bdepthAc;
		// size_t bdepthDc;

		{
			size_t compressed_index = 0;
			auto bpe_debug_input_buffer_callback = [&compressed, &compressed_index]() -> uint64_t {
				// static auto iter = compressed.begin();
				return compressed[compressed_index++];
				};
			ibitwrapper<uint64_t> binput(bpe_debug_input_buffer_callback, stage_limit << 3);

			try {
				bpedecoder.decode(backward_input, binput);
			}
			catch (const ccsds::bpe::byte_limit_exception& ex) {
				std::cout << "Byte limit reached when decoding segment 0, decoded output size: "
					<< compressed_index << std::endl;
			}
		}

		for (ptrdiff_t i = 0; i < backward_input.size; ++i) {
			for (ptrdiff_t j = 1; j < 64; ++j) { // TODO: items per block
				EXPECT_EQ(backward_input.data[i].content[j], precoded[0]->data[i].content[j])
					<< " at segment [0], block [" << i << "], coeff [" << j << "]";
			}
		}

		*(precoded[0]) = backward_input;
		SegmentDisassembler<item_t> postdecoder;
		postdecoder.set_image_width(props.width);
		auto decoded = postdecoder.apply(std::move(precoded));

		size_t out_row_offset = 0;
		for (auto& block : decoded) {
			img_meta meta = block[0].get_meta();
			img_pos fragment_props{ 0 };
			fragment_props.width = meta.width;
			fragment_props.height = meta.height;
			BackwardWaveletTransformer<item_t> bdwt;
			bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
			auto out_fragment = bdwt.apply(block);
			size_t out_fragment_height; // = relu(out_fragment.get_meta().height - 16);
			if (&block != &(decoded.back())) {
				out_fragment_height = relu(out_fragment.get_meta().height - 16);
			}
			else {
				out_fragment_height = out_fragment.get_meta().height;
			}
			output.resize(props.width, out_row_offset + out_fragment_height);
			for (size_t i = 0; i < out_fragment_height; ++i) {
				output[out_row_offset + i].assign(out_fragment[i]);
			}
			out_row_offset += out_fragment_height;
		}
	}

	img_meta input_props = input.get_meta();
	img_meta output_props = output.get_meta();
	ASSERT_EQ(input_props.width, output_props.width);
	ASSERT_EQ(input_props.height, output_props.height);

	for (size_t i = 0; i < props.height; ++i) {
		for (size_t j = 0; j < props.width; ++j) {
			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
		}
	}
}
