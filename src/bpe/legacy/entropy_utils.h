#pragma once

#include "EntropyEncoder.tpp"
#include "EntropyDecoder.tpp"

namespace entropy {
	template <typename T>
	using encoder = EntropyEncoder<T::wordDepth, T::codeOption, typename T::buffer_t, typename T::stream_char_t>;

	template <typename T>
	using decoder = EntropyDecoder<T::wordDepth, T::codeOption, typename T::buffer_t, typename T::stream_char_t>;
};
