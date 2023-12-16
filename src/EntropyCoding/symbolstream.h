#pragma once

#include "isymbolstream.h"
#include "osymbolstream.h"

class symbolstream 
	: virtual public isymbolstream, virtual public osymbolstream {
public:
	symbolstream() = default;
	symbolstream(const content_t &content, size_t size);
};

