#pragma once

#include "dwt.tpp"

//
// Explicit instantiation declarations:

extern template class ForwardWaveletTransformer<int8_t>;
extern template subbands_t<int8_t> ForwardWaveletTransformer<int8_t>::apply<int8_t>(const bitmap<int8_t>& source, const img_pos& frame);
extern template subbands_t<int8_t> ForwardWaveletTransformer<int8_t>::apply<uint8_t>(const bitmap<uint8_t>& source, const img_pos& frame);
extern template subbands_t<int8_t> ForwardWaveletTransformer<int8_t>::apply<int8_t>(bitmap<int8_t>& source);
extern template subbands_t<int8_t> ForwardWaveletTransformer<int8_t>::apply<uint8_t>(bitmap<uint8_t>& source);
extern template void ForwardWaveletTransformer<int8_t>::preprocess_image<int8_t>(bitmap<int8_t>& src);
extern template void ForwardWaveletTransformer<int8_t>::preprocess_image<uint8_t>(bitmap<uint8_t>& src);
//extern template void ForwardWaveletTransformer<int8_t>::transform<int8_t>(const_bitmap_slice<int8_t> source,
//	bitmap_slice<int8_t> hdst, bitmap_slice<int8_t> ldst,
//	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
//extern template void ForwardWaveletTransformer<int8_t>::transform<uint8_t>(const_bitmap_slice<uint8_t> source,
//	bitmap_slice<int8_t> hdst, bitmap_slice<int8_t> ldst,
//	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);

extern template class ForwardWaveletTransformer<int16_t>;
extern template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<int16_t>(const bitmap<int16_t>& source, const img_pos& frame);
extern template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<int8_t>(const bitmap<int8_t>& source, const img_pos& frame);
extern template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<uint16_t>(const bitmap<uint16_t>& source, const img_pos& frame);
extern template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<uint8_t>(const bitmap<uint8_t>& source, const img_pos& frame);
extern template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<int16_t>(bitmap<int16_t>& source);
extern template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<int8_t>(bitmap<int8_t>& source);
extern template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<uint16_t>(bitmap<uint16_t>& source);
extern template subbands_t<int16_t> ForwardWaveletTransformer<int16_t>::apply<uint8_t>(bitmap<uint8_t>& source);
extern template void ForwardWaveletTransformer<int16_t>::preprocess_image<int16_t>(bitmap<int16_t>& src);
extern template void ForwardWaveletTransformer<int16_t>::preprocess_image<int8_t>(bitmap<int8_t>& src);
extern template void ForwardWaveletTransformer<int16_t>::preprocess_image<uint16_t>(bitmap<uint16_t>& src);
extern template void ForwardWaveletTransformer<int16_t>::preprocess_image<uint8_t>(bitmap<uint8_t>& src);
// extern template void ForwardWaveletTransformer<int16_t>::transform<int16_t>(const_bitmap_slice<int16_t> source,
// 	bitmap_slice<int16_t> hdst, bitmap_slice<int16_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
// extern template void ForwardWaveletTransformer<int16_t>::transform<int8_t>(const_bitmap_slice<int8_t> source,
// 	bitmap_slice<int16_t> hdst, bitmap_slice<int16_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
// extern template void ForwardWaveletTransformer<int16_t>::transform<uint16_t>(const_bitmap_slice<uint16_t> source,
// 	bitmap_slice<int16_t> hdst, bitmap_slice<int16_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
// extern template void ForwardWaveletTransformer<int16_t>::transform<uint8_t>(const_bitmap_slice<uint8_t> source,
// 	bitmap_slice<int16_t> hdst, bitmap_slice<int16_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);

extern template class ForwardWaveletTransformer<int32_t>;
extern template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<int32_t>(const bitmap<int32_t>& source, const img_pos& frame);
extern template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<int16_t>(const bitmap<int16_t>& source, const img_pos& frame);
extern template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<uint32_t>(const bitmap<uint32_t>& source, const img_pos& frame);
extern template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<uint16_t>(const bitmap<uint16_t>& source, const img_pos& frame);
extern template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<int32_t>(bitmap<int32_t>& source);
extern template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<int16_t>(bitmap<int16_t>& source);
extern template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<uint32_t>(bitmap<uint32_t>& source);
extern template subbands_t<int32_t> ForwardWaveletTransformer<int32_t>::apply<uint16_t>(bitmap<uint16_t>& source);
extern template void ForwardWaveletTransformer<int32_t>::preprocess_image<int32_t>(bitmap<int32_t>& src);
extern template void ForwardWaveletTransformer<int32_t>::preprocess_image<int16_t>(bitmap<int16_t>& src);
extern template void ForwardWaveletTransformer<int32_t>::preprocess_image<uint32_t>(bitmap<uint32_t>& src);
extern template void ForwardWaveletTransformer<int32_t>::preprocess_image<uint16_t>(bitmap<uint16_t>& src);
// extern template void ForwardWaveletTransformer<int32_t>::transform<int32_t>(const_bitmap_slice<int32_t> source,
// 	bitmap_slice<int32_t> hdst, bitmap_slice<int32_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
// extern template void ForwardWaveletTransformer<int32_t>::transform<int16_t>(const_bitmap_slice<int16_t> source,
// 	bitmap_slice<int32_t> hdst, bitmap_slice<int32_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
// extern template void ForwardWaveletTransformer<int32_t>::transform<uint32_t>(const_bitmap_slice<uint32_t> source,
// 	bitmap_slice<int32_t> hdst, bitmap_slice<int32_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
// extern template void ForwardWaveletTransformer<int32_t>::transform<uint16_t>(const_bitmap_slice<uint16_t> source,
// 	bitmap_slice<int32_t> hdst, bitmap_slice<int32_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);

extern template class ForwardWaveletTransformer<int64_t>;
extern template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<int64_t>(const bitmap<int64_t>& source, const img_pos& frame);
extern template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<int32_t>(const bitmap<int32_t>& source, const img_pos& frame);
extern template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<uint64_t>(const bitmap<uint64_t>& source, const img_pos& frame);
extern template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<uint32_t>(const bitmap<uint32_t>& source, const img_pos& frame);
extern template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<int64_t>(bitmap<int64_t>& source);
extern template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<int32_t>(bitmap<int32_t>& source);
extern template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<uint64_t>(bitmap<uint64_t>& source);
extern template subbands_t<int64_t> ForwardWaveletTransformer<int64_t>::apply<uint32_t>(bitmap<uint32_t>& source);
extern template void ForwardWaveletTransformer<int64_t>::preprocess_image<int64_t>(bitmap<int64_t>& src);
extern template void ForwardWaveletTransformer<int64_t>::preprocess_image<int32_t>(bitmap<int32_t>& src);
extern template void ForwardWaveletTransformer<int64_t>::preprocess_image<uint64_t>(bitmap<uint64_t>& src);
extern template void ForwardWaveletTransformer<int64_t>::preprocess_image<uint32_t>(bitmap<uint32_t>& src);
// extern template void ForwardWaveletTransformer<int64_t>::transform<int64_t>(const_bitmap_slice<int64_t> source,
// 	bitmap_slice<int64_t> hdst, bitmap_slice<int64_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
// extern template void ForwardWaveletTransformer<int64_t>::transform<int32_t>(const_bitmap_slice<int32_t> source,
// 	bitmap_slice<int64_t> hdst, bitmap_slice<int64_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
// extern template void ForwardWaveletTransformer<int64_t>::transform<uint64_t>(const_bitmap_slice<uint64_t> source,
// 	bitmap_slice<int64_t> hdst, bitmap_slice<int64_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);
// extern template void ForwardWaveletTransformer<int64_t>::transform<uint32_t>(const_bitmap_slice<uint32_t> source,
// 	bitmap_slice<int64_t> hdst, bitmap_slice<int64_t> ldst,
// 	ptrdiff_t hdst_drop_offset, ptrdiff_t ldst_drop_offset, bool skip_extension);


extern template class ForwardWaveletTransformer<float>;
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<int32_t>(const bitmap<int32_t>& source, const img_pos& frame);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<int16_t>(const bitmap<int16_t>& source, const img_pos& frame);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<int8_t>(const bitmap<int8_t>& source, const img_pos& frame);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint32_t>(const bitmap<uint32_t>& source, const img_pos& frame);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint16_t>(const bitmap<uint16_t>& source, const img_pos& frame);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint8_t>(const bitmap<uint8_t>& source, const img_pos& frame);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<int32_t>(bitmap<int32_t>& source);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<int16_t>(bitmap<int16_t>& source);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<int8_t>(bitmap<int8_t>& source);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint32_t>(bitmap<uint32_t>& source);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint16_t>(bitmap<uint16_t>& source);
extern template subbands_t<float> ForwardWaveletTransformer<float>::apply<uint8_t>(bitmap<uint8_t>& source);
extern template void ForwardWaveletTransformer<float>::preprocess_image<int32_t>(bitmap<int32_t>& src);
extern template void ForwardWaveletTransformer<float>::preprocess_image<int16_t>(bitmap<int16_t>& src);
extern template void ForwardWaveletTransformer<float>::preprocess_image<int8_t>(bitmap<int8_t>& src);
extern template void ForwardWaveletTransformer<float>::preprocess_image<uint32_t>(bitmap<uint32_t>& src);
extern template void ForwardWaveletTransformer<float>::preprocess_image<uint16_t>(bitmap<uint16_t>& src);
extern template void ForwardWaveletTransformer<float>::preprocess_image<uint8_t>(bitmap<uint8_t>& src);

extern template class ForwardWaveletTransformer<double>;
extern template subbands_t<double> ForwardWaveletTransformer<double>::apply<int64_t>(const bitmap<int64_t>& source, const img_pos& frame);
extern template subbands_t<double> ForwardWaveletTransformer<double>::apply<int32_t>(const bitmap<int32_t>& source, const img_pos& frame);
extern template subbands_t<double> ForwardWaveletTransformer<double>::apply<uint64_t>(const bitmap<uint64_t>& source, const img_pos& frame);
extern template subbands_t<double> ForwardWaveletTransformer<double>::apply<uint32_t>(const bitmap<uint32_t>& source, const img_pos& frame);
extern template subbands_t<double> ForwardWaveletTransformer<double>::apply<int64_t>(bitmap<int64_t>& source);
extern template subbands_t<double> ForwardWaveletTransformer<double>::apply<int32_t>(bitmap<int32_t>& source);
extern template subbands_t<double> ForwardWaveletTransformer<double>::apply<uint64_t>(bitmap<uint64_t>& source);
extern template subbands_t<double> ForwardWaveletTransformer<double>::apply<uint32_t>(bitmap<uint32_t>& source);
extern template void ForwardWaveletTransformer<double>::preprocess_image<int64_t>(bitmap<int64_t>& src);
extern template void ForwardWaveletTransformer<double>::preprocess_image<int32_t>(bitmap<int32_t>& src);
extern template void ForwardWaveletTransformer<double>::preprocess_image<uint64_t>(bitmap<uint64_t>& src);
extern template void ForwardWaveletTransformer<double>::preprocess_image<uint32_t>(bitmap<uint32_t>& src);


extern template class BackwardWaveletTransformer<int8_t>;
extern template bitmap<int8_t> BackwardWaveletTransformer<int8_t>::apply<int8_t>(subbands_t<int8_t>& subbands, size_t top_overlap);
extern template bitmap<uint8_t> BackwardWaveletTransformer<int8_t>::apply<uint8_t>(subbands_t<int8_t>& subbands, size_t top_overlap);

extern template class BackwardWaveletTransformer<int16_t>;
extern template bitmap<int16_t> BackwardWaveletTransformer<int16_t>::apply<int16_t>(subbands_t<int16_t>& subbands, size_t top_overlap);
extern template bitmap<int8_t> BackwardWaveletTransformer<int16_t>::apply<int8_t>(subbands_t<int16_t>& subbands, size_t top_overlap);
extern template bitmap<uint16_t> BackwardWaveletTransformer<int16_t>::apply<uint16_t>(subbands_t<int16_t>& subbands, size_t top_overlap);
extern template bitmap<uint8_t> BackwardWaveletTransformer<int16_t>::apply<uint8_t>(subbands_t<int16_t>& subbands, size_t top_overlap);

extern template class BackwardWaveletTransformer<int32_t>;
extern template bitmap<int32_t> BackwardWaveletTransformer<int32_t>::apply<int32_t>(subbands_t<int32_t>& subbands, size_t top_overlap);
extern template bitmap<int16_t> BackwardWaveletTransformer<int32_t>::apply<int16_t>(subbands_t<int32_t>& subbands, size_t top_overlap);
extern template bitmap<uint32_t> BackwardWaveletTransformer<int32_t>::apply<uint32_t>(subbands_t<int32_t>& subbands, size_t top_overlap);
extern template bitmap<uint16_t> BackwardWaveletTransformer<int32_t>::apply<uint16_t>(subbands_t<int32_t>& subbands, size_t top_overlap);

extern template class BackwardWaveletTransformer<int64_t>;
extern template bitmap<int64_t> BackwardWaveletTransformer<int64_t>::apply<int64_t>(subbands_t<int64_t>& subbands, size_t top_overlap);
extern template bitmap<int32_t> BackwardWaveletTransformer<int64_t>::apply<int32_t>(subbands_t<int64_t>& subbands, size_t top_overlap);
extern template bitmap<uint64_t> BackwardWaveletTransformer<int64_t>::apply<uint64_t>(subbands_t<int64_t>& subbands, size_t top_overlap);
extern template bitmap<uint32_t> BackwardWaveletTransformer<int64_t>::apply<uint32_t>(subbands_t<int64_t>& subbands, size_t top_overlap);


// TODO: fp support:
extern template class BackwardWaveletTransformer<float>;
extern template bitmap<int32_t> BackwardWaveletTransformer<float>::apply<int32_t>(subbands_t<float>& subbands, size_t top_overlap);
extern template bitmap<int16_t> BackwardWaveletTransformer<float>::apply<int16_t>(subbands_t<float>& subbands, size_t top_overlap);
extern template bitmap<int8_t> BackwardWaveletTransformer<float>::apply<int8_t>(subbands_t<float>& subbands, size_t top_overlap);
extern template bitmap<uint32_t> BackwardWaveletTransformer<float>::apply<uint32_t>(subbands_t<float>& subbands, size_t top_overlap);
extern template bitmap<uint16_t> BackwardWaveletTransformer<float>::apply<uint16_t>(subbands_t<float>& subbands, size_t top_overlap);
extern template bitmap<uint8_t> BackwardWaveletTransformer<float>::apply<uint8_t>(subbands_t<float>& subbands, size_t top_overlap);

extern template class BackwardWaveletTransformer<double>;
extern template bitmap<int64_t> BackwardWaveletTransformer<double>::apply<int64_t>(subbands_t<double>& subbands, size_t top_overlap);
extern template bitmap<int32_t> BackwardWaveletTransformer<double>::apply<int32_t>(subbands_t<double>& subbands, size_t top_overlap);
extern template bitmap<uint64_t> BackwardWaveletTransformer<double>::apply<uint64_t>(subbands_t<double>& subbands, size_t top_overlap);
extern template bitmap<uint32_t> BackwardWaveletTransformer<double>::apply<uint32_t>(subbands_t<double>& subbands, size_t top_overlap);

