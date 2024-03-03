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