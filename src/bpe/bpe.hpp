#pragma once

#include "bpe.tpp"


// explicit instantiation section

// encoders instantiated only for machine word size output bit streams
extern template class BitPlaneEncoder<int8_t>;
extern template void BitPlaneEncoder<int8_t>::encode<uintptr_t>(segment<int8_t>& input, obitwrapper<uintptr_t>& output);
extern template void BitPlaneEncoder<int8_t>::kOptimal<int8_t, uintptr_t>(kParams<int8_t> params, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int8_t>::kHeuristic<int8_t, uintptr_t>(kParams<int8_t> params, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int8_t>::kEncode<int8_t, uintptr_t>(kParams<int8_t> params, bool use_heuristic, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int8_t>::encodeBpeStages<uintptr_t>(segment<int8_t>& input, obitwrapper<uintptr_t>& output);

extern template class BitPlaneEncoder<int16_t>;
extern template void BitPlaneEncoder<int16_t>::encode<uintptr_t>(segment<int16_t>& input, obitwrapper<uintptr_t>& output);
extern template void BitPlaneEncoder<int16_t>::kOptimal<int16_t, uintptr_t>(kParams<int16_t> params, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int16_t>::kHeuristic<int16_t, uintptr_t>(kParams<int16_t> params, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int16_t>::kEncode<int16_t, uintptr_t>(kParams<int16_t> params, bool use_heuristic, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int16_t>::encodeBpeStages<uintptr_t>(segment<int16_t>& input, obitwrapper<uintptr_t>& output);

extern template class BitPlaneEncoder<int32_t>;
extern template void BitPlaneEncoder<int32_t>::encode<uintptr_t>(segment<int32_t>& input, obitwrapper<uintptr_t>& output);
extern template void BitPlaneEncoder<int32_t>::kOptimal<int32_t, uintptr_t>(kParams<int32_t> params, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int32_t>::kHeuristic<int32_t, uintptr_t>(kParams<int32_t> params, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int32_t>::kEncode<int32_t, uintptr_t>(kParams<int32_t> params, bool use_heuristic, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int32_t>::encodeBpeStages<uintptr_t>(segment<int32_t>& input, obitwrapper<uintptr_t>& output);

extern template class BitPlaneEncoder<int64_t>;
extern template void BitPlaneEncoder<int64_t>::encode<uintptr_t>(segment<int64_t>& input, obitwrapper<uintptr_t>& output);
extern template void BitPlaneEncoder<int64_t>::kOptimal<int64_t, uintptr_t>(kParams<int64_t> params, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int64_t>::kHeuristic<int64_t, uintptr_t>(kParams<int64_t> params, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int64_t>::kEncode<int64_t, uintptr_t>(kParams<int64_t> params, bool use_heuristic, obitwrapper<uintptr_t>& output_stream);
extern template void BitPlaneEncoder<int64_t>::encodeBpeStages<uintptr_t>(segment<int64_t>& input, obitwrapper<uintptr_t>& output);


// decoders instantiated for 8,32,64 bit input bit streams for every segment data item size
extern template class BitPlaneDecoder<int8_t>;
extern template void BitPlaneDecoder<int8_t>::decode<uint8_t>(segment<int8_t>& output, ibitwrapper<uint8_t>& input);
extern template void BitPlaneDecoder<int8_t>::kReverse<int8_t, uint8_t>(kParams<int8_t>& params, ibitwrapper<uint8_t>& input_stream);
extern template void BitPlaneDecoder<int8_t>::kDecode<int8_t, uint8_t>(kParams<int8_t>& params, ibitwrapper<uint8_t>& input_stream);
extern template void BitPlaneDecoder<int8_t>::decodeBpeStages<uint8_t>(segment<int8_t>& output, ibitwrapper<uint8_t>& input);
extern template void BitPlaneDecoder<int8_t>::decode<uint32_t>(segment<int8_t>& output, ibitwrapper<uint32_t>& input);
extern template void BitPlaneDecoder<int8_t>::kReverse<int8_t, uint32_t>(kParams<int8_t>& params, ibitwrapper<uint32_t>& input_stream);
extern template void BitPlaneDecoder<int8_t>::kDecode<int8_t, uint32_t>(kParams<int8_t>& params, ibitwrapper<uint32_t>& input_stream);
extern template void BitPlaneDecoder<int8_t>::decodeBpeStages<uint32_t>(segment<int8_t>& output, ibitwrapper<uint32_t>& input);
extern template void BitPlaneDecoder<int8_t>::decode<uint64_t>(segment<int8_t>& output, ibitwrapper<uint64_t>& input);
extern template void BitPlaneDecoder<int8_t>::kReverse<int8_t, uint64_t>(kParams<int8_t>& params, ibitwrapper<uint64_t>& input_stream);
extern template void BitPlaneDecoder<int8_t>::kDecode<int8_t, uint64_t>(kParams<int8_t>& params, ibitwrapper<uint64_t>& input_stream);
extern template void BitPlaneDecoder<int8_t>::decodeBpeStages<uint64_t>(segment<int8_t>& output, ibitwrapper<uint64_t>& input);

extern template class BitPlaneDecoder<int16_t>;
extern template void BitPlaneDecoder<int16_t>::decode<uint8_t>(segment<int16_t>& output, ibitwrapper<uint8_t>& input);
extern template void BitPlaneDecoder<int16_t>::kReverse<int16_t, uint8_t>(kParams<int16_t>& params, ibitwrapper<uint8_t>& input_stream);
extern template void BitPlaneDecoder<int16_t>::kDecode<int16_t, uint8_t>(kParams<int16_t>& params, ibitwrapper<uint8_t>& input_stream);
extern template void BitPlaneDecoder<int16_t>::decodeBpeStages<uint8_t>(segment<int16_t>& output, ibitwrapper<uint8_t>& input);
extern template void BitPlaneDecoder<int16_t>::decode<uint32_t>(segment<int16_t>& output, ibitwrapper<uint32_t>& input);
extern template void BitPlaneDecoder<int16_t>::kReverse<int16_t, uint32_t>(kParams<int16_t>& params, ibitwrapper<uint32_t>& input_stream);
extern template void BitPlaneDecoder<int16_t>::kDecode<int16_t, uint32_t>(kParams<int16_t>& params, ibitwrapper<uint32_t>& input_stream);
extern template void BitPlaneDecoder<int16_t>::decodeBpeStages<uint32_t>(segment<int16_t>& output, ibitwrapper<uint32_t>& input);
extern template void BitPlaneDecoder<int16_t>::decode<uint64_t>(segment<int16_t>& output, ibitwrapper<uint64_t>& input);
extern template void BitPlaneDecoder<int16_t>::kReverse<int16_t, uint64_t>(kParams<int16_t>& params, ibitwrapper<uint64_t>& input_stream);
extern template void BitPlaneDecoder<int16_t>::kDecode<int16_t, uint64_t>(kParams<int16_t>& params, ibitwrapper<uint64_t>& input_stream);
extern template void BitPlaneDecoder<int16_t>::decodeBpeStages<uint64_t>(segment<int16_t>& output, ibitwrapper<uint64_t>& input);

extern template class BitPlaneDecoder<int32_t>;
extern template void BitPlaneDecoder<int32_t>::decode<uint8_t>(segment<int32_t>& output, ibitwrapper<uint8_t>& input);
extern template void BitPlaneDecoder<int32_t>::kReverse<int32_t, uint8_t>(kParams<int32_t>& params, ibitwrapper<uint8_t>& input_stream);
extern template void BitPlaneDecoder<int32_t>::kDecode<int32_t, uint8_t>(kParams<int32_t>& params, ibitwrapper<uint8_t>& input_stream);
extern template void BitPlaneDecoder<int32_t>::decodeBpeStages<uint8_t>(segment<int32_t>& output, ibitwrapper<uint8_t>& input);
extern template void BitPlaneDecoder<int32_t>::decode<uint32_t>(segment<int32_t>& output, ibitwrapper<uint32_t>& input);
extern template void BitPlaneDecoder<int32_t>::kReverse<int32_t, uint32_t>(kParams<int32_t>& params, ibitwrapper<uint32_t>& input_stream);
extern template void BitPlaneDecoder<int32_t>::kDecode<int32_t, uint32_t>(kParams<int32_t>& params, ibitwrapper<uint32_t>& input_stream);
extern template void BitPlaneDecoder<int32_t>::decodeBpeStages<uint32_t>(segment<int32_t>& output, ibitwrapper<uint32_t>& input);
extern template void BitPlaneDecoder<int32_t>::decode<uint64_t>(segment<int32_t>& output, ibitwrapper<uint64_t>& input);
extern template void BitPlaneDecoder<int32_t>::kReverse<int32_t, uint64_t>(kParams<int32_t>& params, ibitwrapper<uint64_t>& input_stream);
extern template void BitPlaneDecoder<int32_t>::kDecode<int32_t, uint64_t>(kParams<int32_t>& params, ibitwrapper<uint64_t>& input_stream);
extern template void BitPlaneDecoder<int32_t>::decodeBpeStages<uint64_t>(segment<int32_t>& output, ibitwrapper<uint64_t>& input);

extern template class BitPlaneDecoder<int64_t>;
extern template void BitPlaneDecoder<int64_t>::decode<uint8_t>(segment<int64_t>& output, ibitwrapper<uint8_t>& input);
extern template void BitPlaneDecoder<int64_t>::kReverse<int64_t, uint8_t>(kParams<int64_t>& params, ibitwrapper<uint8_t>& input_stream);
extern template void BitPlaneDecoder<int64_t>::kDecode<int64_t, uint8_t>(kParams<int64_t>& params, ibitwrapper<uint8_t>& input_stream);
extern template void BitPlaneDecoder<int64_t>::decodeBpeStages<uint8_t>(segment<int64_t>& output, ibitwrapper<uint8_t>& input);
extern template void BitPlaneDecoder<int64_t>::decode<uint32_t>(segment<int64_t>& output, ibitwrapper<uint32_t>& input);
extern template void BitPlaneDecoder<int64_t>::kReverse<int64_t, uint32_t>(kParams<int64_t>& params, ibitwrapper<uint32_t>& input_stream);
extern template void BitPlaneDecoder<int64_t>::kDecode<int64_t, uint32_t>(kParams<int64_t>& params, ibitwrapper<uint32_t>& input_stream);
extern template void BitPlaneDecoder<int64_t>::decodeBpeStages<uint32_t>(segment<int64_t>& output, ibitwrapper<uint32_t>& input);
extern template void BitPlaneDecoder<int64_t>::decode<uint64_t>(segment<int64_t>& output, ibitwrapper<uint64_t>& input);
extern template void BitPlaneDecoder<int64_t>::kReverse<int64_t, uint64_t>(kParams<int64_t>& params, ibitwrapper<uint64_t>& input_stream);
extern template void BitPlaneDecoder<int64_t>::kDecode<int64_t, uint64_t>(kParams<int64_t>& params, ibitwrapper<uint64_t>& input_stream);
extern template void BitPlaneDecoder<int64_t>::decodeBpeStages<uint64_t>(segment<int64_t>& output, ibitwrapper<uint64_t>& input);