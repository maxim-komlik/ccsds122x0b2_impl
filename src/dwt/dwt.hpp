#pragma once

#include "dwt.tpp"

//
// Explicit instantiation declarations:

extern template class ForwardWaveletTransformer<int8_t>;
extern template class ForwardWaveletTransformer<int16_t>;
extern template class ForwardWaveletTransformer<int32_t>;
extern template class ForwardWaveletTransformer<int64_t>;

// TODO: fp support:
// extern template class ForwardWaveletTransformer<float>;
// extern template class ForwardWaveletTransformer<double>;


extern template class BackwardWaveletTransformer<int8_t>;
extern template class BackwardWaveletTransformer<int16_t>;
extern template class BackwardWaveletTransformer<int32_t>;
extern template class BackwardWaveletTransformer<int64_t>;

// TODO: fp support:
// extern template class BackwardWaveletTransformer<float>;
// extern template class BackwardWaveletTransformer<double>;
