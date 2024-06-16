#include "symbolstream.h"

#include <vector>

#include "entropy_types.h"

symbolstream::symbolstream(std::vector<content_t>&& src) :
		content(std::move(src)), size(0), currentIndex(0) { }

const symbolstream::content_t* const symbolstream::data() const {
	return this->content.data();
}

size_t symbolstream::bytelength() const {
	return this->content.size();
}

size_t symbolstream::_size() const {
	return this->size;
}

void symbolstream::reset() {
	this->restart();
	this->size = 0;
	this->content.clear();
}

void symbolstream::restart() {
	this->currentIndex = 0;
}

size_t symbolstream::get() const {
	content_t ret_value =
		(((content[this->currentIndex >> 1]) &
			((content_t)((-(int)((~(this->currentIndex)) & 0x01)) ^ 0x0f))) >>
			((-(int)((~(this->currentIndex)) & 0x01)) & 4));
	++(this->currentIndex);
	return (size_t)(ret_value);
}

symbolstream::operator bool() const {
	return currentIndex != size;
}

bool symbolstream::operator!() const {
	// currentIndex == size;
	return !((bool)*this);
}

void symbolstream::put(content_t item) {
	++(this->size);
	if (this->size > (this->content.size() << 1)) {
		this->content.push_back(item << 4);
	}
	else {
		this->content[(this->size - 1) >> 1] ^= item;
	}
}
