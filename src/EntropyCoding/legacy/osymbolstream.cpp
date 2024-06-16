#include "osymbolstream.h"

void osymbolstream::put(size_t item) {
    ++(this->size);
    if (this->size > (this->content.size() << 1)) {
		this->content.push_back(item << 4);
    } else {
		this->content[(this->size - 1) >> 1] ^= item;
    }
};