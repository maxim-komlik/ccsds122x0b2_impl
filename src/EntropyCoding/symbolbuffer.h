#pragma once

#include <vector>

#include "entropy_types.h"

class symbolbuffer {
	using content_t = dense_vlw_t::type;
	std::vector<content_t> content;
	size_t next_read_index = 0;

	// TODO: check if should have knowledge about contained symbols
	size_t size_of_symbol = 0; 
	// This could be used for another memory management techinique, but benefit is 
	// not clear. Otherwise it is only used to compute accumulative bitlen of 
	// all the symbols contained in the buffer

public:
	~symbolbuffer() = default;

	symbolbuffer(size_t max_symbol_count, size_t symbol_size);

	operator bool() const;

	void append(content_t item);
	size_t next(); // modifies the state of the buffer (next observable symbol is different)
	void reset();
	void restart();
	const content_t* const data() const;

	size_t buffer_capacity() const;		// allocated capacity of internal buffer in bytes
	size_t size_symbols() const;	// size in symbols
	size_t bitlength() const;		// accumulated bitlen of all contained symbols

	size_t symbol_size() const;		// bitlen of a single symbol
};
