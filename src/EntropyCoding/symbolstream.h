#pragma once

#include <vector>

#include "entropy_types.h"

class symbolstream {
	using content_t = dense_vlw_t::type;
	std::vector<content_t> content;
	size_t size = 0;
	mutable size_t currentIndex = 0;

public:
	~symbolstream() = default;
	symbolstream() = default;

	symbolstream(std::vector<content_t>&& src);

	operator bool() const;
	bool operator!() const;

	void put(content_t item);
	size_t get() const;
	void reset();
	void restart();
	const content_t* const data() const;
	// TODO: check interface below and usage
	size_t bytelength() const;
	size_t _size() const;
};
