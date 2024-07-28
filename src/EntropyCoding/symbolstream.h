#pragma once

#include <vector>

#include "entropy_types.h"

class symbolstream {
	using content_t = dense_vlw_t::type; // TODO: review type dependency
	std::vector<content_t> content;
	size_t size = 0;
	mutable size_t next_read_index = 0;

	size_t size_of_symbol = 0; // TODO: check if should have knowledge about contained symbols
	// This could be used for another memory management techinique, but benefit is 
	// not clear. Otherwise it is only used to compute accumulative bitlen of 
	// all the symbols contained in the buffer

public:
	~symbolstream() = default;
	symbolstream() = default;

	symbolstream(size_t symbol_count, size_t symbol_size);
	symbolstream(std::vector<content_t>&& src);

	operator bool() const;
	bool operator!() const;

	void put(content_t item);
	size_t get() const; // TODO: check const correctness, related to mutable private member
	void reset();
	void restart();
	const content_t* const data() const;
	// TODO: check interface below and usage
	size_t bytelength() const;
	size_t _size() const;

	size_t buffer_capacity() const;		// allocated capacity of internal buffer
	size_t size_bytes() const;	// used size of internal buffer in bytes
	size_t size_symbols() const;	// size in symbols
	size_t bitsize_symbols() const;		// accumulated bitlen of all contained symbols

	size_t symbol_size() const;		// bitlen of a single symbol
};
