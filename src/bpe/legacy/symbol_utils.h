#pragma once

#include "SymbolEncoder.tpp"
#include "SymbolDecoder.tpp"

namespace symbol {
	template <typename T>
	using encoder = SymbolEncoder<T::wordDepth, T::codeOption, typename T::buffer_t, typename T::stream_char_t>;

	template <typename T>
	using decoder = SymbolDecoder<T::wordDepth, T::codeOption, typename T::buffer_t, typename T::stream_char_t>;
};