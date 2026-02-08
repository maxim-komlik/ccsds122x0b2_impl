#include "file_sink.hpp"

template <typename T>
file_buffer_wrapper<T>::file_buffer_wrapper(std::filebuf& stream) : buffer(buffer_size), file_stream(stream) {}

template <typename T>
void file_buffer_wrapper<T>::store_word(T value) {
	this->buffer[this->write_buffer_index] = value;
	++(this->write_buffer_index);

	[[unlikely]]
	if ((this->write_buffer_index & this->buffer_index_mask) == 0) {
		this->flush_buffer();
	}
	this->write_buffer_index &= this->buffer_index_mask;
}

template <typename T>
void file_buffer_wrapper<T>::reset_buffer() {
	constexpr T reset_item_value = 0;
	std::fill(this->buffer.begin(), this->buffer.end(), reset_item_value);
	this->write_buffer_index = 0;
}

template <typename T>
void file_buffer_wrapper<T>::flush_buffer() {
	if (std::endian::big != std::endian::native) {
		std::transform(this->buffer.cbegin(), this->buffer.cbegin() + this->write_buffer_index,
			this->buffer.begin(), byteswap<decltype(this->buffer)::value_type>);
	}
	// this->file_stream.write(reinterpret_cast<char*>(this->buffer.data()), this->write_buffer_index * sizeof(decltype(this->buffer)::value_type));
	auto wirtten_count = this->file_stream.sputn(reinterpret_cast<char*>(this->buffer.data()),
		this->write_buffer_index * sizeof(decltype(this->buffer)::value_type));
	// TODO: error handling on partial write / written_count < target size?
}


template <typename T>
T file_buffer_wrapper<T>::load_word() {
	[[unlikely]]
	if ((this->write_buffer_index & this->buffer_index_mask) == 0) {
		this->fill_buffer();
	}

	[[unlikely]]
	if (this->data_max_index == this->write_buffer_index) {
		// TODO: throw end of data exception?
	}

	T result = this->buffer[this->write_buffer_index];
	++(this->write_buffer_index);
	this->write_buffer_index &= this->buffer_index_mask;
	return result;
}

template <typename T>
void file_buffer_wrapper<T>::fill_buffer() {
	constexpr size_t buffer_capacity = buffer_size * sizeof(decltype(this->buffer)::value_type);
	auto read_count = this->file_stream.sgetn(reinterpret_cast<char*>(this->buffer.data()), 
		buffer_capacity);
	if (read_count < buffer_capacity) {
		size_t value_type_mask = sizeof(decltype(this->buffer)::value_type) - 1;
		std::fill_n(reinterpret_cast<char*>(this->buffer.data()) + read_count,
			buffer_capacity - read_count, char{});
		this->data_max_index = (read_count + value_type_mask) >> std::bit_width(value_type_mask);
	}

	if constexpr (std::endian::native != std::endian::big) {
		std::transform(this->buffer.cbegin(), this->buffer.cbegin() + this->write_buffer_index,
			this->buffer.begin(), byteswap<decltype(this->buffer)::value_type>);
	}
}

