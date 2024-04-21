#pragma once

template <size_t wordDepth, int setType, bool reverse = false>
struct SymbolCode;

// fictive
template <>
struct SymbolCode<1, 0> {
	size_t mapping[1 << 1] = {
		0x00,	// 0
		0x01,	// 1
	};
};

template <>
struct SymbolCode<2, 0> {
	size_t mapping[1 << 2] = {
		0x00,	// 0
		0x02,	// 1 
		0x01, 	// 2
		0x03	// 3
	};
};

template <>
struct SymbolCode<2, 0, true> {
	size_t mapping[1 << 2] = {
		0x00,	// 0
		0x02,	// 1 
		0x01, 	// 2
		0x03	// 3
	};
};

template <>
struct SymbolCode<3, 0> {
	size_t mapping[1 << 3] = {
		0x01,	// 0
		0x04,	// 1
		0x00, 	// 2
		0x05,	// 3
		0x02,	// 4
		0x06,	// 5
		0x03, 	// 6
		0x07	// 7
	};
};

template <>
struct SymbolCode<3, 0, true> {
	size_t mapping[1 << 3] = {
		0x02, 	// 0
		0x00,	// 1
		0x04,	// 2
		0x06, 	// 3
		0x01,	// 4
		0x03,	// 5
		0x05,	// 6
		0x07	// 7
	};
};

template <>
struct SymbolCode<3, 1> {
	size_t mapping[1 << 3] = {
		0x08,	// 0	-
		0x03,	// 1
		0x00, 	// 2
		0x04,	// 3
		0x01,	// 4
		0x05,	// 5
		0x02, 	// 6
		0x06	// 7
	};
};

template <>
struct SymbolCode<3, 1, true> {
	size_t mapping[1 << 3] = {
		0x02, 	// 0
		0x04,	// 1
		0x06, 	// 2
		0x01,	// 3
		0x03,	// 4
		0x05,	// 5
		0x07,	// 6
		0x08,	// 0	-
	};
};

template <>
struct SymbolCode<4, 0> {
	size_t mapping[1 << 4] = {
		0x0a,	// 0
		0x01,	// 1
		0x03, 	// 2
		0x06,	// 3
		0x02,	// 4
		0x05,	// 5
		0x09, 	// 6
		0x0c,	// 7
		0x00,	// 8
		0x08,	// 9
		0x07, 	// 10
		0x0d,	// 11
		0x04,	// 12
		0x0e,	// 13
		0x0b, 	// 14
		0x0f	// 15
	};
};

template <>
struct SymbolCode<4, 0, true> {
	size_t mapping[1 << 4] = {
		0x08,	// 0
		0x01,	// 1
		0x04,	// 2
		0x02, 	// 3
		0x0c,	// 4
		0x05,	// 5
		0x03,	// 6
		0x0a, 	// 7
		0x09,	// 8
		0x06, 	// 9
		0x00,	// 10
		0x0e, 	// 11
		0x07,	// 12
		0x0b,	// 13
		0x0d,	// 14
		0x0f	// 15
	};
};

template <>
struct SymbolCode<4, 1> {
	size_t mapping[1 << 4] = {
		0x10,	// 0	-
		0x01,	// 1
		0x03, 	// 2
		0x06,	// 3
		0x02,	// 4
		0x05,	// 5
		0x09, 	// 6
		0x0b,	// 7
		0x00,	// 8
		0x08,	// 9
		0x07, 	// 10
		0x0c,	// 11
		0x04,	// 12
		0x0d,	// 13
		0x0a, 	// 14
		0x0e	// 15
	};
};

template <>
struct SymbolCode<4, 1, true> {
	size_t mapping[1 << 4] = {
		0x00,	// 0
		0x01,	// 1
		0x02,	// 2
		0x03, 	// 3
		0x04,	// 4
		0x05,	// 5
		0x06,	// 6
		0x07, 	// 7
		0x08,	// 8
		0x09, 	// 9
		0x0a, 	// 10
		0x0b,	// 11
		0x0c,	// 12
		0x0d,	// 13
		0x0e,	// 14
		0x10,	// 0	-
	};
};