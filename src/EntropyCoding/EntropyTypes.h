#pragma once

namespace code {
	namespace {

		template <size_t _wordDepth, size_t _codeOption, typename _buffer_t, typename _stream_char_t = wchar_t>
		struct _code_type {
			static const size_t wordDepth = _wordDepth;
			static const size_t codeOption = _codeOption;
			typedef _buffer_t buffer_t;
			typedef _stream_char_t stream_char_t;
		};
	}

	template <size_t wordDepth, size_t codeOption, typename buffer_t, typename stream_char_t = wchar_t>
	using code_type = _code_type<wordDepth, codeOption, buffer_t, stream_char_t>;
};

struct _variadic_code_item {
	size_t length;
	size_t code;
};

typedef _variadic_code_item SymbolEncoding;
typedef _variadic_code_item shift_params;

template <typename T = size_t>
struct _vlw_t {
	using type = T;

	T length;
	T value;

	template <typename D>
	operator _vlw_t<D>() {
		return _vlw_t<D>{ (D)(this->length), (D)(this->value) };
	};
};

using dense_vlw_t = _vlw_t<uint8_t>;
using vlw_t = _vlw_t<size_t>;
// 
// uint8_t is not proper type because symbol encodings have points 8 bit length.
// When coding to obitwrapper, shift operator for uint8_t and shit amount 8 bit 
// is UB.
using symbol_encoding = _vlw_t<uint_fast16_t>;


template <typename int_type>
struct char_type;

template <>
struct char_type <unsigned int> {
	typedef char32_t type;
};

template <>
struct char_type <unsigned short> {
	typedef char16_t type;
};