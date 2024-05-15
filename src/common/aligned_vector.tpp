#pragma once

#include <algorithm>
#include <memory>

template <typename T, size_t alignment>
class aligned_vector;

template <typename T, size_t alignment>
void swap(aligned_vector<T, alignment>& first, aligned_vector<T, alignment>& second) noexcept;

template <typename T, size_t alignment = 16>
class aligned_vector {
	T* m_begin;
	T* m_rawdata;
	size_t m_length;
	size_t m_offset;
	size_t m_rawdata_length;

	static constexpr size_t palignment = alignment * sizeof(T);
public:
	~aligned_vector();
	aligned_vector(aligned_vector&& other) noexcept;
	aligned_vector(const aligned_vector& other);

	aligned_vector& operator=(aligned_vector&& other) noexcept;
	aligned_vector& operator=(const aligned_vector& other);

	aligned_vector();
	aligned_vector(size_t length, T* origin);
	aligned_vector(size_t length, size_t offset = 16);

	// declaration order matters
	friend void swap<>(aligned_vector& first, aligned_vector& second) noexcept;
	void swap(aligned_vector& other) noexcept;

	void assign(T value);
	void fill(size_t length, T* origin);
	void resize(size_t length);
	size_t size() const noexcept;
	// T operator[](size_t index) const;
	T& operator[](size_t index) const;
	inline T* data() const;
private:
	void __reallocate();
};

template <typename T, size_t alignment>
aligned_vector<T, alignment>::~aligned_vector() {
	delete[] this->m_rawdata;
}

template <typename T, size_t alignment>
aligned_vector<T, alignment>::aligned_vector(aligned_vector&& other) noexcept :
		m_begin(other.m_begin), m_rawdata(other.m_rawdata),
		m_length(other.m_length), m_offset(other.m_offset), m_rawdata_length(other.m_rawdata_length) {
	other.m_rawdata = nullptr;
	other.m_begin = nullptr;
	other.m_rawdata_length = 0;
}

template <typename T, size_t alignment>
aligned_vector<T, alignment>::aligned_vector(const aligned_vector& other) : 
		m_length(other.m_length), m_offset(other.m_offset) {
	this->m_rawdata_length = std::min(other.m_rawdata_length,
		((other.m_length + alignment - 1) & (~(alignment - 1))) + 2 * other.m_offset);

	this->m_rawdata = new T[this->m_rawdata_length + (alignment - 1)];	// adds multiple of sizeof(T)
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->palignment - 1))
		& (~(this->palignment - 1)))) + this->m_offset;

	for (size_t i = 0; i < this->m_length; i += alignment) {
		for (size_t j = 0; j < alignment; ++j) {
			this->m_begin[i + j] = other.m_begin[i + j];
		}
	}
}

template <typename T, size_t alignment>
aligned_vector<T, alignment>& aligned_vector<T, alignment>::operator= (aligned_vector<T, alignment>&& other) noexcept {
	T* temp = this->m_rawdata;
	this->m_rawdata = other.m_rawdata;
	other.m_rawdata = temp;

	this->m_begin = other.m_begin;
	this->m_length = other.m_length;
	this->m_offset = other.m_offset;
	this->m_rawdata_length = other.m_rawdata_length;
	return *this;
}

template <typename T, size_t alignment>
aligned_vector<T, alignment>& aligned_vector<T, alignment>::operator= (const aligned_vector<T, alignment>& other) {
	if (this != &other) {
		this->m_length = other.m_length;
		this->m_offset = other.m_offset;

		if (this->m_rawdata_length < (other.m_length + other.m_offset)) {
			this->m_rawdata_length = std::min(other.m_rawdata_length,
				((other.m_length + alignment - 1) & (~(alignment - 1))) + 2 * other.m_offset);
			this->__reallocate();
		}

		for (size_t i = 0; i < this->m_length; i += alignment) {
			for (size_t j = 0; j < alignment; ++j) {
				this->m_begin[i + j] = other.m_begin[i + j];
			}
		}
	}
	return *this;
}

template <typename T, size_t alignment>
aligned_vector<T, alignment>::aligned_vector() : m_rawdata(nullptr), m_begin(nullptr), m_rawdata_length(0) {}

template <typename T, size_t alignment>
aligned_vector<T, alignment>::aligned_vector(size_t length, T* origin) {
	this->m_length = length;
	// that ensures every new row starts with alligned address
	length = (length + alignment - 1) & (~(alignment - 1));
	this->m_offset = alignment;
	this->m_rawdata_length = length + 2 * alignment;

	this->m_rawdata = new T[this->m_rawdata_length + (alignment - 1)];	// adds multiple of sizeof(T)
	// physical memory alignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->palignment - 1))
		& (~(this->palignment - 1)))) + this->m_offset;

	ptrdiff_t i;
	for (i = 0; i < ((ptrdiff_t)length) - alignment; i += alignment) {
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			this->m_begin[i + j] = origin[i + j];
		}
	}
	for (; i < length; ++i) {
		this->m_begin[i] = origin[i];
	}
}

template <typename T, size_t alignment>
aligned_vector<T, alignment>::aligned_vector(size_t length, size_t offset) {
	this->m_length = length;
	// that ensures every new row starts with alligned address
	length = (length + alignment - 1) & (~(alignment - 1));
	offset = (offset + alignment) & (~(alignment - 1));
	this->m_offset = offset;
	this->m_rawdata_length = length + 2 * offset;

	this->m_rawdata = new T[this->m_rawdata_length + (alignment - 1)];	// adds multiple of sizeof(T)
	// physical memory alignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->palignment - 1))
		& (~(this->palignment - 1)))) + this->m_offset;
}

template <typename T, size_t alignment>
void aligned_vector<T, alignment>::__reallocate() {
	delete[] this->m_rawdata;
	this->m_rawdata = new T[this->m_rawdata_length + (alignment - 1)];	// adds multiple of sizeof(T)
	// physical memory alignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->palignment - 1))
		& (~(this->palignment - 1)))) + this->m_offset;
}

template <typename T, size_t alignment>
void aligned_vector<T, alignment>::swap(aligned_vector<T, alignment>& other) noexcept {
	swap(*this, other);
}

// friend swap function
template <typename T, size_t alignment>
void swap(aligned_vector<T, alignment>& first, aligned_vector<T, alignment>& second) noexcept {
	using std::swap;
	swap(first.m_begin, second.m_begin);
	swap(first.m_rawdata, second.m_rawdata);
	swap(first.m_length, second.m_length);
	swap(first.m_offset, second.m_offset);
	swap(first.m_rawdata_length, second.m_rawdata_length);
}

namespace std {
	template <typename T, size_t alignment>
	void swap(aligned_vector<T, alignment>& first, aligned_vector<T, alignment>& second) noexcept {
		// swap(first, second); // will cause recursion as the current namespace std:: will be searched first
		first.swap(second);
	}
}

template <typename T, size_t alignment>
void aligned_vector<T, alignment>::assign(T value) {
	// constexpr size_t vmask = alignment - 1;
	// size_t vtail_start = this->m_length & (~vmask);
	// size_t vtail_size = this->m_length & vmask;
	for (ptrdiff_t i = 0; i < this->m_length; i += alignment) {
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			this->m_begin[i + j] = value;
		}
	}
}

template <typename T, size_t alignment>
size_t aligned_vector<T, alignment>::size() const noexcept {
	return this->m_length;
}

template <typename T, size_t alignment>
T& aligned_vector<T, alignment>::operator[](size_t index) const {
	// TODO: boundary checks
	return this->m_begin[index];
}

template <typename T, size_t alignment>
inline
T* aligned_vector<T, alignment>::data() const {
	return std::assume_aligned<this->palignment>(this->m_begin);
}

template <typename T, size_t alignment>
void aligned_vector<T, alignment>::resize(size_t length) {
	if (this->m_rawdata_length < (length + this->m_offset)) { // check operator=
		this->m_rawdata_length = ((length + alignment - 1) & (~(alignment - 1))) + 2 * this->m_offset;
		this->__reallocate();
	}
	this->m_length = length;
}