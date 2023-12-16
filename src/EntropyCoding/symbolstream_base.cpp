#include "symbolstream_base.h"

symbolstream_base::symbolstream_base(const symbolstream_base::content_t &content, size_t size)
	: content(content), size(size) {};

size_t symbolstream_base::capacity() const {
	return this->content.size();
};

const symbolstream_base::buffer_t* const symbolstream_base::data() const {
	return this->content.data();
}