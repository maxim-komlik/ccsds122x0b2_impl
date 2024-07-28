#include "symbolbuffer.h"

#include <vector>

#include "entropy_types.h"

symbolbuffer::symbolbuffer(size_t max_symbol_count, size_t symbol_size) : 
		next_read_index(0), size_of_symbol(symbol_size) {
	this->content.reserve(max_symbol_count);
}

symbolbuffer::operator bool() const {
	return (this->content.size() != this->next_read_index);
}

void symbolbuffer::reset(){
	this->restart();
	this->content.clear();
}

void symbolbuffer::restart() {
	this->next_read_index = 0;
}

void symbolbuffer::append(content_t item) {
	this->content.push_back(item);
}

size_t symbolbuffer::next() {
	content_t return_value = this->content[this->next_read_index];
	++(this->next_read_index);
	return return_value;
}

const symbolbuffer::content_t* const symbolbuffer::data() const {
	return this->content.data();
}

size_t symbolbuffer::buffer_capacity() const {
	return this->content.capacity();
}

size_t symbolbuffer::size_symbols() const {
	return this->content.size();
}

size_t symbolbuffer::bitlength() const {
	return (this->content.size() * this->size_of_symbol);
}

size_t symbolbuffer::symbol_size() const {
	return this->size_of_symbol;
}
