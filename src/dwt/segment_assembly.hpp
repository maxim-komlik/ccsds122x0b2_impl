#pragma once

#include "segment_assembly.tpp"

//
// Explicit instantiation declarations:

extern template class SegmentAssembler<int8_t>;
extern template class SegmentAssembler<int16_t>;
extern template class SegmentAssembler<int32_t>;
extern template class SegmentAssembler<int64_t>;

extern template class SegmentAssembler<float>;
extern template class SegmentAssembler<double>;


extern template class SegmentDisassembler<int8_t>;
extern template class SegmentDisassembler<int16_t>;
extern template class SegmentDisassembler<int32_t>;
extern template class SegmentDisassembler<int64_t>;

extern template class SegmentDisassembler<float>;
extern template class SegmentDisassembler<double>;
