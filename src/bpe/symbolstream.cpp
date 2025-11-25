#include "symbolstream.h"

#include <vector>

#include "entropy_types.h"

symbolstream::symbolstream(std::vector<content_t>&& src) :
		content(std::move(src)), size(0), next_read_index(0) { }

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
	this->next_read_index = 0;
}

size_t symbolstream::get() const {
	using signed_index_t = std::make_signed_t<decltype(this->next_read_index)>;

	content_t ret_value =
		(this->content[this->next_read_index >> 1] &
			((content_t)((-(signed_index_t)((~(this->next_read_index)) & 0x01)) ^ 0x0f))) >>
		((-(signed_index_t)((~(this->next_read_index)) & 0x01)) & 4); /// masked, either 0 or 4

	++(this->next_read_index);
	return ret_value;
}

symbolstream::operator bool() const {
	return next_read_index != size;
}

bool symbolstream::operator!() const { // actually this is redundant, operator! causes contextual conversion to bool
	return !((bool)*this);
}

void symbolstream::put(content_t item) {
	++(this->size);
	if (this->size > (this->content.size() << 1)) {
		this->content.push_back(item << 4);
	} else {
		this->content[(this->size - 1) >> 1] ^= item;
	}
}
