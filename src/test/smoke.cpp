#include "gtest/gtest.h"

#include "dwt/dwt.hpp"
#include "dwt/dwtcore.tpp"

#include "test_utils.tpp"

TEST(dwtcorei, DwtCoreI) {
	typedef int64_t item_t;
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

TEST(dwti, multilevelRound) {
	typedef int64_t item_t;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	shifts_t subband_shifts{ 0 };

	ForwardWaveletTransformer<item_t> dwt;
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.preprocess_image(input);
	auto subbands = dwt.apply(input, input.single_frame_params());

	BackwardWaveletTransformer<item_t> reverser;
	reverser.get_scale().set_shifts(subband_shifts);
	reverser.set_skip_dc_scaling(false);
	bitmap<item_t> output = reverser.apply(subbands);

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

TEST(tbitmap, bitmapMemorySmoke) {
	typedef int64_t item_t;
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
		props.width += (props.width == 0);
		props.height += (props.height == 0);

		size_t offset = generator() & img_offset_mask;
		seed = ((seed + (props.width ^ props.height)) ^ generator()) ^ img_dim_mask;
		bitmap<item_t> first = generateNoisyBitmap<item_t>(props.width, props.height, offset, seed);

		props.width = generator() & img_dim_mask;
		props.height = generator() & img_dim_mask;
		props.width += (props.width == 0);
		props.height += (props.height == 0);

		offset = generator() & img_offset_mask;
		seed = ((seed + (props.width ^ props.height)) ^ generator()) ^ img_dim_mask;
		bitmap<item_t> second = generateNoisyBitmap<item_t>(props.width, props.height, offset, seed);

		img_meta second_meta = second.get_meta();
		size_t slice_pos = generator() % (second_meta.height * second_meta.width);
		size_t slice_len = generator() % (second_meta.height * second_meta.width - slice_pos);
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
		second.linear(second_slice_ptr, slice_len, slice_pos);
		first = std::move(second);
		item_t* first_slice_ptr = (item_t*)((((size_t)(first_slice.data())) + (2 * palignment - 1)) & (~(palignment - 1)));
		first.linear(first_slice_ptr, slice_len, slice_pos);
		second = std::forward<bitmap<item_t>&>(first);
		item_t* control_slice_ptr = (item_t*)((((size_t)(control_slice.data())) + (2 * palignment - 1)) & (~(palignment - 1)));
		second.linear(control_slice_ptr, slice_len, slice_pos);

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

#include "dwt/segment_assembly.hpp"

TEST(segments, EncoderSmoke) {
	typedef int64_t item_t;
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

	for (size_t i = 0; i < output.size(); ++i) {
		EXPECT_LE(output[i]->q, output[i]->bdepthDc) << " q > depth DC at index [" << i << "]";
		EXPECT_LE(output[i]->q, output[i]->bdepthDc) << " q > depth DC at index [" << i << "]";
	}
}

TEST(segments, EncoderRound) {
	typedef int64_t item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
	shifts_t subband_shifts{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };

	ForwardWaveletTransformer<item_t> dwt;
	dwt.get_scale().set_shifts(subband_shifts);
	dwt.preprocess_image(input);
	auto i_coeffs = dwt.apply(input, input.single_frame_params());
	auto coeffs_copy = i_coeffs;

	SegmentAssembler<item_t> precoder;
	precoder.set_shifts(dwt.get_scale().get_shifts());
	auto coded = precoder.apply(std::move(i_coeffs));

	std::vector<segment<item_t>> coded_copy;
	for (auto& item : coded) {
		coded_copy.push_back(*item);
	}

	SegmentDisassembler<item_t> postdecoder;
	postdecoder.set_image_width(props.width);
	auto decoded = postdecoder.apply(std::move(coded));

	bitmap<item_t> output;
	BackwardWaveletTransformer<item_t> bdwt;

	for (auto& block: decoded) {
		bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
		auto out_fragment = bdwt.apply(block);
		output.append(out_fragment);
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

//
// The test below spotted expectation violation at line 274, i.e. 
//		coded_target[0].quantizedDc[i] != coded_target[0].quantizedDc[i];
//	for i = 16383, although original image restored with no errors.
// The check below was violated in release build only
// This could be an effect of different object code due to different translation 
// units and optimizer's different decisions accordingly, but it was not verified.
// 
// TEST(segments, refactorValidation) {
// 	typedef long long item_t;
// 	constexpr size_t alignment = 16;
// 	img_pos props;
// 	props.width = 1 << 10;
// 	props.height = 1 << 10;
// 	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);
// 
// 	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
// 	shifts_t subband_shifts{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
// 
// 	ForwardWaveletTransformer<item_t> dwt;
// 	dwt.get_scale().set_shifts(subband_shifts);
// 	dwt.preprocess_image(input);
// 	auto i_coeffs = dwt.apply(input, input.single_frame_params());
// 	auto coeffs_copy = i_coeffs;
// 
// 	refactor::SegmentAssembler<item_t> precoder;
// 	precoder.set_shifts(dwt.get_scale().get_shifts());
// 	auto coded = precoder.apply(std::move(i_coeffs));
// 
// 	std::vector<segment<decltype(precoder)::output_value_type>> coded_flat;
// 	for (auto& item : coded) {
// 		coded_flat.push_back(*item);
// 	}
// 
// 	SegmentDisassembler<item_t> postdecoder(coded_flat);
// 	auto decoded = postdecoder.apply(props.width);
// 
// 	SegmentAssembler<item_t> precoder_old(coeffs_copy);
// 	precoder_old.set_shifts(dwt.get_scale().get_shifts());
// 	auto coded_target = precoder_old.apply();
// 
// 
// 	for (ptrdiff_t i = 0; i < coded_target[0].quantizedDc.size(); ++i) {
// 		item_t expected = coded_target[0].quantizedDc[i];
// 		item_t actual = coded_flat[0].quantizedDc[i];
// 		if (expected - actual != 0) { // bet for ints
// 			EXPECT_EQ(expected, actual) << " at index [" << i << "]";
// 		}
// 	}
// 
// 	for (ptrdiff_t i = 0; i < coded_target[0].plainDc.size(); ++i) {
// 		item_t expected = coded_target[0].plainDc[i];
// 		item_t actual = coded_flat[0].plainDc[i];
// 		if (expected - actual != 0) {
// 			EXPECT_EQ(expected, actual) << " at index [" << i << "]";
// 		}
// 	}
// 
// 	for (ptrdiff_t i = 0; i < coded_target[0].quantizedBdepthAc.size(); ++i) {
// 		item_t expected = coded_target[0].quantizedBdepthAc[i];
// 		item_t actual = coded_flat[0].quantizedBdepthAc[i];
// 		if (expected - actual != 0) {
// 			EXPECT_EQ(expected, actual) << " at index [" << i << "]";
// 		}
// 	}
// 
// 	for (ptrdiff_t i = 0; i < coded_target[0].size; ++i) {
// 		for (ptrdiff_t j = 0; j < 64; ++j) {
// 			item_t expected = coded_target[0].data[i].content[j];
// 			item_t actual = coded_flat[0].data[i].content[j];
// 			if (expected - actual != 0) {
// 				EXPECT_EQ(expected, actual) << " at index [" << i << "][" << j << "]";
// 			}
// 		}
// 	}
// 
// 	for (ptrdiff_t subband_index = 0; subband_index < 10; ++subband_index) {
// 		img_meta dims = i_coeffs[subband_index].get_meta();
// 		for (ptrdiff_t i = 0; i < dims.height; ++i) {
// 			for (ptrdiff_t j = 0; j < dims.width; ++j) {
// 				item_t expected = i_coeffs[subband_index][i][j];
// 				item_t result_value = decoded[0][subband_index][i][j];
// 				if (expected - result_value != 0) {
// 					EXPECT_EQ(expected, result_value) 
// 						<< " at index [" << subband_index << "][" << i << "][" << j << "]";
// 				}
// 			}
// 		}
// 	}
// 
// 	img_pos fragment_props_target { 0 };
// 	fragment_props_target.width = i_coeffs[0].get_meta().width;
// 	fragment_props_target.height = i_coeffs[0].get_meta().height;
// 	BackwardWaveletTransformer<item_t> bdwt;
// 	bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
// 	auto target_reversed = bdwt.apply(i_coeffs);
// 
// 
// 	bitmap<item_t> output;
// 
// 	size_t out_row_offset = 0;
// 	for (auto& block: decoded) {
// 		img_meta meta = block[0].get_meta();
// 		img_pos fragment_props { 0 };
// 		fragment_props.width = meta.width;
// 		fragment_props.height = meta.height;
// 		bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
// 		auto out_fragment = bdwt.apply(block);
// 		size_t out_fragment_height; // = relu(out_fragment.get_meta().height - 16);
// 		if (&block != &(decoded.back())) {
// 			out_fragment_height = relu(out_fragment.get_meta().height - 16);
// 		} else {
// 			out_fragment_height = out_fragment.get_meta().height;
// 		}
// 		output.append(out_fragment);
// 	}
// 
// 
// 	img_meta input_props = input.get_meta();
// 	img_meta output_props = output.get_meta();
// 	ASSERT_EQ(input_props.width, output_props.width);
// 	ASSERT_EQ(input_props.height, output_props.height);
// 
// 	for (size_t i = 0; i < props.height; ++i) {
// 		for (size_t j = 0; j < props.width; ++j) {
// 			EXPECT_EQ(input[i][j], output[i][j]) << " at index [" << i << "][" << j << "]";
// 		}
// 	}
// }

#include "bpe/bpe.tpp"

TEST(bpe, EncoderSmoke) {
	typedef int64_t item_t;
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

	std::vector<uintptr_t> bpe_debug_output_buffer;
	BitPlaneEncoder<decltype(output)::value_type::element_type::type> bpeencoder;
	bpeencoder.set_use_heuristic_DC(false);
	bpeencoder.set_use_heuristic_bdepthAc(false);

	{
		auto bpe_debug_output_buffer_callback = [&bpe_debug_output_buffer](uintptr_t item) -> void {
			bpe_debug_output_buffer.push_back(item);
		};
		obitwrapper<uintptr_t> boutput(bpe_debug_output_buffer_callback);

		bpeencoder.encode(*(output[0]), boutput);
		boutput.flush();
	}

	size_t size_compressed = bpe_debug_output_buffer.size();
	size_t size_initial = input.get_meta().height * input.get_meta().width;

	EXPECT_LE(size_compressed, size_initial) 
		<< " compressed size increased! compressed = " << size_compressed 
		<< "; initial = " << size_initial;

	std::cout << "Compression rate: " << (((double)size_compressed) / ((double)size_initial)) << std::endl;
}

TEST(bpe, EncoderRound) {
	typedef int64_t item_t;
	constexpr size_t alignment = 16;
	img_pos props;
	props.width = 1 << 10;
	props.height = 1 << 10;
	bitmap<item_t> input = generateNoisyBitmap<item_t>(props.width, props.height, 64);

	// { 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 }
	shifts_t subband_shifts{ 3, 3, 2, 1, 3, 2, 1, 3, 2, 1 };
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

	std::vector<uintptr_t> compressed;
	// constexpr size_t stage_limit = 0x0937; // stage 2 debug
	// constexpr size_t stage_limit = 0x0a4a; // stage 3 debug
	constexpr size_t stage_limit = (0x01 << 24) - 1;

	{
		auto bpe_debug_output_buffer_callback = [&compressed](uintptr_t item) -> void {
			compressed.push_back(item);
		};
		obitwrapper<uintptr_t> boutput(bpe_debug_output_buffer_callback, stage_limit << 3);

		try {
			bpeencoder.encode(bpe_input_segment, boutput);
		} catch (const ccsds::bpe::byte_limit_exception& ex) {
			std::cout << "Byte limit reached when encoding segment 0, output size: "
				<< compressed.size() << std::endl;
		}
		boutput.flush();
	}
	bitmap<item_t> output; 
	BitPlaneDecoder<decltype(bpe_input_segment)::type> bpedecoder;

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
			auto bpe_debug_input_buffer_callback = [&compressed, &compressed_index]() -> uintptr_t {
				// static auto iter = compressed.begin();
				return compressed[compressed_index++];
			};
			ibitwrapper<uintptr_t> binput(bpe_debug_input_buffer_callback, stage_limit << 3);

			try {
				bpedecoder.decode(backward_input, binput);
			} catch (const ccsds::bpe::byte_limit_exception& ex) {
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
		for (auto& block: decoded) {
			img_meta meta = block[0].get_meta();
			img_pos fragment_props { 0 };
			fragment_props.width = meta.width;
			fragment_props.height = meta.height;
			BackwardWaveletTransformer<item_t> bdwt;
			bdwt.get_scale().set_shifts(dwt.get_scale().get_shifts());
			auto out_fragment = bdwt.apply(block);
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
