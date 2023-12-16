#pragma once

#include <vector>

class symbolstream_base {
protected:
	// each symbol holds 4 bit (2 symbols per byte)
	typedef unsigned char buffer_t;
	typedef std::vector<buffer_t> content_t;
	size_t size = 0;
	content_t content;

public: 
	symbolstream_base() = default;
	symbolstream_base(const content_t &content, size_t size);
protected:
	virtual ~symbolstream_base() = default;
public:
	size_t capacity() const;
	const buffer_t* const data() const;
};

