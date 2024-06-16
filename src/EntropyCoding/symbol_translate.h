#pragma once

#include <array>
#include <tuple>

#include "entropy_types.h"
#include "symbol_code.tpp"

struct SymbolForwardTranslator {
private:
	std::tuple<
		// larger structures first
		std::tuple<symbol_code<4, 0>, symbol_code<4, 1>>,
		std::tuple<symbol_code<3, 0>, symbol_code<3, 1>>,
		std::tuple<symbol_code<2, 0>>,
		// fictive one
		std::tuple<symbol_code<1, 0>>> codes;

	std::array<std::array<size_t*, 5>, 3> mapping = { {
		{
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<2>(codes)).mapping,
			std::get<0>(std::get<1>(codes)).mapping,
			std::get<0>(std::get<0>(codes)).mapping
		},
		{
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<2>(codes)).mapping,
			std::get<1>(std::get<1>(codes)).mapping,
			std::get<0>(std::get<0>(codes)).mapping
		},
		{
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<2>(codes)).mapping,
			std::get<0>(std::get<1>(codes)).mapping,
			std::get<1>(std::get<0>(codes)).mapping
		}
	} };

public:
	// nor copyable nor movable due to mapping member containing pointers
	SymbolForwardTranslator() = default; // causes compiler not to define copy/move ctors implicitly
	SymbolForwardTranslator& operator=(const SymbolForwardTranslator& other) = delete;

	// const values for translate template parameter
	static const size_t types_H_codeparam = 0x02;
	static const size_t tran_H_codeparam = 0x02;
	static const size_t tran_D_codeparam = 0x01;

	template <size_t codeparam = 0>
	void translate(dense_vlw_t& vlw) {
		vlw.value =
			this->mapping[codeparam][vlw.length][vlw.value];
	}
};


struct SymbolBackwardTranslator {
private:
	std::tuple<
		// larger structures first
		std::tuple<symbol_code<4, 0, true>, symbol_code<4, 1, true>>,
		std::tuple<symbol_code<3, 0, true>, symbol_code<3, 1, true>>,
		std::tuple<symbol_code<2, 0, true>>,
		// fictive one
		std::tuple<symbol_code<1, 0>>> codes;

	std::array<std::array<size_t*, 5>, 3> mapping = { {
		{
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<2>(codes)).mapping,
			std::get<0>(std::get<1>(codes)).mapping,
			std::get<0>(std::get<0>(codes)).mapping
		},
		{
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<2>(codes)).mapping,
			std::get<1>(std::get<1>(codes)).mapping,
			std::get<0>(std::get<0>(codes)).mapping
		},
		{
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<3>(codes)).mapping,
			std::get<0>(std::get<2>(codes)).mapping,
			std::get<0>(std::get<1>(codes)).mapping,
			std::get<1>(std::get<0>(codes)).mapping
		}
	} };

public:
	// nor copyable nor movable due to mapping member containing pointers
	SymbolBackwardTranslator() = default; // causes compiler not to define copy/move ctors implicitly
	SymbolBackwardTranslator& operator=(const SymbolBackwardTranslator& other) = delete;

	// const values for translate template parameter
	static const size_t types_H_codeparam = 0x02;
	static const size_t tran_H_codeparam = 0x02;
	static const size_t tran_D_codeparam = 0x01;

	template <size_t codeparam = 0>
	void translate(vlw_t& vlw) {
		// TODO: implement validation, unknown data source.
		vlw.value =
			this->mapping[codeparam][vlw.length][vlw.value];
	}
};
