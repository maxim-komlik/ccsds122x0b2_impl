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

TEST(dwti, multilevelRound) {
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

	shifts_t subband_shifts{ 0 };

	ForwardWaveletTransformer<item_t> dwt(props);
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.apply(input);
	auto i_coeffs = dwt.get_subbands();

	img_pos reverser_props = props;
	// reverser_props.width /= 8;
	// reverser_props.height /= 8;
	BackwardWaveletTransformer<item_t> reverser(reverser_props, false);
	reverser.get_scale().set_shifts(subband_shifts);
	reverser.set_skip_dc_scaling(false);
	reverser.set_subbands(i_coeffs);
	bitmap<item_t> output = reverser.apply();

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

TEST(tbitmap, DISABLED_bitmapMemorySmoke) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
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

		img_meta second_meta = second.get_meta();
		size_t slice_x = generator() % second_meta.width;
		size_t slice_y = generator() % second_meta.height;
		size_t slice_len = generator() % (second_meta.width * (second_meta.height - slice_y) - slice_x);
		std::vector<item_t> first_slice(slice_len + 3 * alignment);
		std::vector<item_t> second_slice(slice_len + 3 * alignment);
		std::vector<item_t> control_slice(slice_len + 3 * alignment);

		// get linear slice second
		// first <-(move) second
		// get same linear slice
		// second <-(copy) first
		// compare slices
		// compare second to slice

		constexpr size_t palignment = alignment * sizeof(item_t);
		item_t* second_slice_ptr = (item_t*)((((size_t)(second_slice.data())) + (2 * palignment - 1)) & (~(palignment - 1)));
		second.linear(second_slice_ptr, slice_len, slice_x, slice_y);
		first = std::move(second);
		item_t* first_slice_ptr = (item_t*)((((size_t)(first_slice.data())) + (2 * palignment - 1)) & (~(palignment - 1)));
		first.linear(first_slice_ptr, slice_len, slice_x, slice_y);
		second = std::forward<bitmap<item_t>&>(first);
		item_t* control_slice_ptr = (item_t*)((((size_t)(control_slice.data())) + (2 * palignment - 1)) & (~(palignment - 1)));
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
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	shifts_t subband_shifts{ 0 };

	ForwardWaveletTransformer<item_t> dwt(props);
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.apply(input);
	auto i_coeffs = dwt.get_subbands();

	SegmentPreCoder<item_t> precoder(i_coeffs);
	precoder.set_shifts(subband_shifts);
	auto output = precoder.apply();

	for (size_t i = 0; i < output.size(); ++i) {
		EXPECT_LE(output[i].q, output[i].bdepthDc) << " q > depth DC at index [" << i << "]";
		EXPECT_LE(output[i].q, output[i].bdepthDc) << " q > depth DC at index [" << i << "]";
	}
}

TEST(segments, PrecoderRound) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
	shifts_t subband_shifts{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

	ForwardWaveletTransformer<item_t> dwt(props);
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.apply(input);
	auto i_coeffs = dwt.get_subbands();

	SegmentPreCoder<item_t> precoder(i_coeffs);
	precoder.set_shifts(dwt.get_scale().get_shifts());
	auto coded = precoder.apply();
	SegmentPostDecoder<item_t> postdecoder(coded);
	// TODO: DEBUG: precoder needs subband weights to compute q
	auto decoded = postdecoder.apply(props.width);
	bitmap<item_t> output;

	size_t out_row_offset = 0;
	for (auto& block: decoded) {
		// for (ptrdiff_t i = 0; i < block.size(); ++i) {
		// 	block[i] = block[i].transpose();
		// }
		img_meta meta = block[0].get_meta();
		img_pos fragment_props { 0 };
		fragment_props.width = meta.width;
		fragment_props.height = meta.height;
		BackwardWaveletTransformer<item_t> bdwt(fragment_props);
		bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
		bdwt.set_subbands(block);
		auto out_fragment = bdwt.apply();
		size_t out_fragment_height; // = relu(out_fragment.get_meta().height - 16);
		if (&block != &(decoded.back())) {
			out_fragment_height = relu(out_fragment.get_meta().height - 16);
		} else {
			out_fragment_height = out_fragment.get_meta().height;
		}
		output.resize(props.width, out_row_offset + out_fragment_height);
		for (size_t i = 0; i < out_fragment_height; ++i) {
			output[out_row_offset + i].assign(out_fragment[i]);
		}
		out_row_offset += out_fragment_height;
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

#include "../EntropyCoding/BitPlaneEncoder.tpp"

TEST(bpe, EncoderSmoke) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	shifts_t subband_shifts{ 0 };

	ForwardWaveletTransformer<item_t> dwt(props);
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.apply(input);
	auto i_coeffs = dwt.get_subbands();

	SegmentPreCoder<item_t> precoder(i_coeffs);
	precoder.set_shifts(subband_shifts);
	auto output = precoder.apply();

	std::vector<uint64_t> bpe_debug_output_buffer;

	{
		auto bpe_debug_output_buffer_callback = [&bpe_debug_output_buffer](uint64_t item) -> void {
			bpe_debug_output_buffer.push_back(item);
		};
		obitwrapper<uint64_t> boutput(bpe_debug_output_buffer_callback);

		__encode(output[0], boutput);

		// TODO: review below
		// here boutput dtor gets called and flushes current buffer, pushing
		// padding to the output container as well.
	}

	size_t size_compressed = bpe_debug_output_buffer.size();
	size_t size_initial = input.get_meta().length;

	EXPECT_LE(size_compressed, size_initial) 
		<< " compressed size increased! compressed = " << size_compressed 
		<< "; initial = " << size_initial;

	std::cout << "Compression rate: " << (((double)size_compressed) / ((double)size_initial)) << std::endl;
}

TEST(bpe, EncoderRound) {
	typedef long long item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
	shifts_t subband_shifts{ 3, 3, 2, 1, 3, 2, 1, 3, 2, 1 };
	// shifts_t subband_shifts{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	ForwardWaveletTransformer<item_t> dwt(props);
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.apply(input);
	auto i_coeffs = dwt.get_subbands();

	SegmentPreCoder<item_t> precoder(i_coeffs);
	precoder.set_shifts(dwt.get_scale().get_shifts());
	auto precoded = precoder.apply();

	auto bpe_input_segment = precoded[0];

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
			__encode(bpe_input_segment, boutput);
		} catch (const ccsds::bpe::byte_limit_exception& ex) {
			std::cout << "Byte limit reached when encoding segment 0, output size: "
				<< compressed.size() << std::endl;
		}

		// here boutput dtor gets called and flushes current buffer, pushing
		// padding to the output container as well.
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
				__decode(backward_input, binput);
			} catch (const ccsds::bpe::byte_limit_exception& ex) {
				std::cout << "Byte limit reached when decoding segment 0, decoded output size: "
					<< compressed_index << std::endl;
			}
		}

		for (ptrdiff_t i = 0; i < backward_input.size; ++i) {
			for (ptrdiff_t j = 1; j < 64; ++j) { // TODO: items per block
				EXPECT_EQ(backward_input.data[i].content[j], precoded[0].data[i].content[j]) 
					<< " at segment [0], block [" << i << "], coeff [" << j << "]";
			}
		}

		precoded[0] = backward_input;
		SegmentPostDecoder<item_t> postdecoder(precoded);
		auto decoded = postdecoder.apply(props.width);

		size_t out_row_offset = 0;
		for (auto& block: decoded) {
			img_meta meta = block[0].get_meta();
			img_pos fragment_props { 0 };
			fragment_props.width = meta.width;
			fragment_props.height = meta.height;
			BackwardWaveletTransformer<item_t> bdwt(fragment_props);
			bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
			bdwt.set_subbands(block);
			auto out_fragment = bdwt.apply();
			size_t out_fragment_height; // = relu(out_fragment.get_meta().height - 16);
			if (&block != &(decoded.back())) {
				out_fragment_height = relu(out_fragment.get_meta().height - 16);
			} else {
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

TEST(bpe, Sparse) {
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

	ForwardWaveletTransformer<item_t> dwt(props);
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.apply(input);
	auto i_coeffs = dwt.get_subbands();

	SegmentPreCoder<item_t> precoder(i_coeffs);
	precoder.set_shifts(dwt.get_scale().get_shifts());
	auto precoded = precoder.apply();

	auto bpe_input_segment = precoded[0];

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
			__encode(bpe_input_segment, boutput);
		}
		catch (const ccsds::bpe::byte_limit_exception& ex) {
			std::cout << "Byte limit reached when encoding segment 0, output size: "
				<< compressed.size() << std::endl;
		}

		// here boutput dtor gets called and flushes current buffer, pushing
		// padding to the output container as well.
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
				__decode(backward_input, binput);
			}
			catch (const ccsds::bpe::byte_limit_exception& ex) {
				std::cout << "Byte limit reached when decoding segment 0, decoded output size: "
					<< compressed_index << std::endl;
			}
		}

		for (ptrdiff_t i = 0; i < backward_input.size; ++i) {
			for (ptrdiff_t j = 1; j < 64; ++j) { // TODO: items per block
				EXPECT_EQ(backward_input.data[i].content[j], precoded[0].data[i].content[j])
					<< " at segment [0], block [" << i << "], coeff [" << j << "]";
			}
		}

		precoded[0] = backward_input;
		SegmentPostDecoder<item_t> postdecoder(precoded);
		auto decoded = postdecoder.apply(props.width);

		size_t out_row_offset = 0;
		for (auto& block : decoded) {
			img_meta meta = block[0].get_meta();
			img_pos fragment_props{ 0 };
			fragment_props.width = meta.width;
			fragment_props.height = meta.height;
			BackwardWaveletTransformer<item_t> bdwt(fragment_props);
			bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
			bdwt.set_subbands(block);
			auto out_fragment = bdwt.apply();
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

TEST(dwti, Sparse) {
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

	ForwardWaveletTransformer<item_t> dwt(props);
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.apply(input);
	auto i_coeffs = dwt.get_subbands();

	BackwardWaveletTransformer<item_t> bdwt(props, false);
	bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
	bdwt.set_skip_dc_scaling(false);
	bdwt.set_subbands(i_coeffs);
	bitmap<item_t> output = bdwt.apply();

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

TEST(segments, Sparse) {
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

	ForwardWaveletTransformer<item_t> dwt(props);
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.apply(input);
	auto i_coeffs = dwt.get_subbands();

	SegmentPreCoder<item_t> precoder(i_coeffs);
	precoder.set_shifts(dwt.get_scale().get_shifts());
	auto precoded = precoder.apply();

	bitmap<item_t> output;

	{
		SegmentPostDecoder<item_t> postdecoder(precoded);
		auto decoded = postdecoder.apply(props.width);

		size_t out_row_offset = 0;
		for (auto& block : decoded) {
			img_meta meta = block[0].get_meta();
			img_pos fragment_props{ 0 };
			fragment_props.width = meta.width;
			fragment_props.height = meta.height;
			BackwardWaveletTransformer<item_t> bdwt(fragment_props);
			bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
			bdwt.set_skip_dc_scaling(false);
			bdwt.set_subbands(block);
			auto out_fragment = bdwt.apply();
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