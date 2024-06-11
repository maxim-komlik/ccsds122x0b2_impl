#pragma once

#include <algorithm>
#include <vector>

struct img_meta {	// per-element location
	size_t stride;
	size_t offset;
	size_t width;
	size_t height;
	size_t depth;
	size_t length;
};

template <typename T, size_t alignment = 16>
class bitmap;

template <typename T, size_t alignment = 16>
class bitmap_row {
	T* const row_start;
	size_t row_width;
public:
	bitmap_row(bitmap_row&& other) = default;
	bitmap_row& operator=(bitmap_row&& other) = default;

	// TODO: check if needed to declare copy members as deleted
	// bitmap_row(const bitmap_row&) = delete;
	// bitmap_row& operator=(const bitmap_row& other) = delete;

	// inaccurate content assignment, vectorizable
	bitmap_row& assign(const bitmap_row& source);
	// Fill content assignement
	bitmap_row& assign(T val);

	T* const ptr() const;
	size_t width() const;
	T& operator[](size_t index) const;
private:
	friend class bitmap<T, alignment>;
	bitmap_row(T* data, size_t width);
};

template <typename T, size_t alignment>
class bitmap {
	T* m_begin;
	T* m_rawdata;
	img_meta m_locator;
	size_t m_rawdata_length;

	static constexpr size_t palignment = alignment * sizeof(T);
public:
	~bitmap();
	bitmap(bitmap&& other) noexcept;
	bitmap(const bitmap& other);

	bitmap& operator=(bitmap&& other) noexcept;
	bitmap& operator=(bitmap& other);

	bitmap();
	bitmap(size_t width, size_t height, T* origin);
	bitmap(size_t width, size_t height, size_t offset = 16);

	void swap(bitmap& other) noexcept;
	friend void swap(bitmap& first, bitmap& second);

	void fill(size_t width, size_t height, T* origin);
	void resize(size_t width, size_t height);
	void shrink(size_t height) noexcept;
	bitmap<T> transpose() const;
	void linear(T* dst, size_t length, size_t x, size_t y) const noexcept;
	// void linear(T* dst, size_t length, size_t linear_index) const noexcept;
	template <typename D>
	void linear(D* dst, size_t length, size_t linear_index) const noexcept;

	bitmap_row<T> operator[](size_t index) const;
	bitmap& operator<<=(size_t shift_amount); // TODO: refactor into operator <<=
	bitmap& operator>>=(size_t shift_amount); // TODO: refactor into operator >>=

	img_meta get_meta() const;
private:
	void __reallocate();
};

template <typename T, size_t alignment>
void overlapBitmaps(const bitmap<T, alignment>& src, bitmap<T, alignment>& dst,
	size_t overlap_height);

// Implementatios section
// bitmap class template implementation

template <typename T, size_t alignment>
bitmap<T, alignment>::~bitmap() {
	delete[] this->m_rawdata;
}

template <typename T, size_t alignment>
bitmap<T, alignment>::bitmap(bitmap&& other) noexcept :
		m_begin(other.m_begin), m_rawdata(other.m_rawdata),
		m_locator(std::move(other.m_locator)), m_rawdata_length(other.m_rawdata_length) {
	other.m_rawdata = nullptr;
	other.m_begin = nullptr;
	other.m_rawdata_length = 0;
}

template <typename T, size_t alignment>
bitmap<T, alignment>::bitmap(const bitmap& other) : m_locator(other.m_locator) {
	this->m_rawdata_length = std::min(other.m_locator.length + other.m_locator.offset, 
		other.m_rawdata_length);

	this->m_rawdata = new T[this->m_rawdata_length + (alignment - 1)];	// adds multiple of sizeof(T)
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->palignment - 1)) 
		& (~(this->palignment - 1)))) + this->m_locator.offset;

	for (size_t i = 0; i < this->m_locator.length; i += alignment) {
		for (size_t j = 0; j < alignment; ++j) {
			this->m_begin[i + j] = other.m_begin[i + j];
		}
	}
}

template <typename T, size_t alignment>
bitmap<T, alignment>& bitmap<T, alignment>::operator= (bitmap<T, alignment>&& other) noexcept {
	T* temp = this->m_rawdata;
	this->m_rawdata = other.m_rawdata;
	other.m_rawdata = temp;

	this->m_begin = other.m_begin;
	this->m_locator = std::move(other.m_locator);
	this->m_rawdata_length = other.m_rawdata_length; // + other.m_locator.offset;

	// other.m_rawdata = nullptr; // TODO: potential memory leak?
	return *this;
}

template <typename T, size_t alignment>
bitmap<T, alignment>& bitmap<T, alignment>::operator= (bitmap<T, alignment>& other) {
	if (this != &other) {
		this->m_locator = other.m_locator;

		if (this->m_rawdata_length < (other.m_locator.length + other.m_locator.offset)) {
			this->m_rawdata_length = std::min(other.m_locator.length + other.m_locator.offset, 
				other.m_rawdata_length);
			this->__reallocate();
		}

		for (size_t i = 0; i < this->m_locator.length; i += alignment) {
			for (size_t j = 0; j < alignment; ++j) {
				this->m_begin[i + j] = other.m_begin[i + j];
			}
		}
	}
	return *this;
}

template <typename T, size_t alignment>
bitmap<T, alignment>::bitmap() : m_rawdata(nullptr), m_begin(nullptr), m_rawdata_length(0) {}

template <typename T, size_t alignment>
bitmap<T, alignment>::bitmap(size_t width, size_t height, T* origin) {
	this->m_locator.width = width;
	width = (width + alignment - 1) & (~(alignment - 1));		//pdu - per data unit
	this->m_locator.offset = alignment; 	//pdu
	this->m_locator.stride = width + 2 * alignment; 	//pdu
	this->m_locator.height = height;
	this->m_locator.depth = 1;
	this->m_locator.length = this->m_locator.height * this->m_locator.stride; 	//pdu
	this->m_rawdata_length = this->m_locator.length + this->m_locator.offset;	//pdu

	this->m_rawdata = new T[this->m_rawdata_length + (alignment - 1)];	// adds multiple of sizeof(T)
	// physical memory alignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->palignment - 1)) 
		& (~(this->palignment - 1)))) + this->m_locator.offset;	//((T*)ppb) + pdu - per physical bytes

	for (size_t row = 0; row < height; ++row) {
		// this->m_locator.width stores original width value
		T* v_src = origin + row * this->m_locator.width;
		T* v_dst = this->m_begin + row * this->m_locator.stride;
		size_t i;
		for (i = 0; i < /*v_bound*/ this->m_locator.width; i += alignment) {
			v_dst[i] = v_src[i];
		}
	}
}

template <typename T, size_t alignment>
bitmap<T, alignment>::bitmap(size_t width, size_t height, size_t offset) {
	// need original width of the image to perform linear()
	this->m_locator.width = width;
	// that ensures every new row starts with alligned address
	width = (width + alignment - 1) & (~(alignment - 1));
	offset = (offset + alignment) & (~(alignment - 1));
	this->m_locator.offset = offset;
	this->m_locator.stride = width + 2 * offset;
	this->m_locator.height = height;
	this->m_locator.depth = 1;
	this->m_locator.length = this->m_locator.height * this->m_locator.stride;
	this->m_rawdata_length = this->m_locator.length + this->m_locator.offset;

	this->m_rawdata = new T[this->m_rawdata_length + (alignment - 1)];	// adds multiple of sizeof(T)
	// physical memory alignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->palignment - 1)) 
		& (~(this->palignment - 1)))) + this->m_locator.offset;
}

template <typename T, size_t alignment>
void bitmap<T, alignment>::swap(bitmap<T, alignment>& other) noexcept {
	swap(*this, other);
}

template <typename T, size_t alignment>
void bitmap<T, alignment>::fill(size_t width, size_t height, T* origin) {
	this->m_locator.width = width;
	width = (width + alignment - 1) & (~(alignment - 1));
	this->m_locator.offset = alignment;
	this->m_locator.stride = width + 2 * alignment;
	this->m_locator.height = height;
	this->m_locator.length = this->m_locator.height * this->m_locator.stride;

	if (this->m_rawdata_length < (this->m_locator.length + this->m_locator.offset)) {
		this->m_rawdata_length = this->m_locator.length + this->m_locator.offset;
		this->__reallocate();
	}

	for (size_t row = 0; row < height; ++row) {
		// this->m_locator.width stores original width value
		T* v_src = origin + row * this->m_locator.width;
		T* v_dst = this->m_begin + row * this->m_locator.stride;
		for (ptrdiff_t i = 0; i < this->m_locator.width; i += alignment) {
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				v_dst[i+j] = v_src[i+j];
			}
		}
	}
}

template <typename T, size_t alignment>
void bitmap<T, alignment>::resize(size_t width, size_t height) {
	this->m_locator.width = width;
	width = (width + alignment - 1) & (~(alignment - 1));
	this->m_locator.offset = alignment;
	this->m_locator.stride = width + 2 * alignment;
	this->m_locator.height = height;
	this->m_locator.depth = 1;
	this->m_locator.length = this->m_locator.height * this->m_locator.stride;

	if (this->m_rawdata_length < (this->m_locator.length + this->m_locator.offset)) {
		this->m_rawdata_length = this->m_locator.length + this->m_locator.offset;
		this->__reallocate();
	}
}

template <typename T, size_t alignment>
void bitmap<T, alignment>::shrink(size_t height) noexcept {
	this->m_locator.height = height;
}

template <typename T, size_t alignment>
bitmap<T> bitmap<T, alignment>::transpose() const {
	bitmap<T> transposed(this->m_locator.height, this->m_locator.width, this->m_locator.offset);
	for (size_t i = 0; i < this->m_locator.height; i += alignment) {
		for (size_t k = 0; k < this->m_locator.width; ++k) {
			bitmap_row<T> dstrow = transposed[k];
			for (size_t j = 0; j < alignment; ++j) {
				dstrow[i + j] = (*this)[i + j][k];
			}
		}
	}
	return transposed;
}

template <typename T, size_t alignment>
void bitmap<T, alignment>::linear(T* dst, size_t length, size_t x, size_t y) const noexcept {
	// Requires dst to be large enough to handle *length* elements + rest of alignment unit
	// at the end + should be preceded by alignment-size buffer. Otherwise blame yourself.
	
	// size_t row_shift = this->m_locator.width & (alignment - 1);
	// size_t allign_shift = (y * row_shift + x) & (alignment - 1);
	// size_t row_count = (length + x) / this->m_locator.width;
	// 
	// size_t x_bound = std::min(this->m_locator.width - x, length);
	// T* row_base = this->m_begin + y * this->m_locator.stride;
	// T* src = row_base + x;
	// for (; row_count >= 0; --row_count) {
	// 	for (; x_bound > 0; x_bound -= alignment, dst += alignment, src += alignment) {
	// 		for (size_t i = 0; i < alignment; ++i) {
	// 			dst[allign_shift + i] = src[i];
	// 		}
	// 	}
	// 
	// 	allign_shift += row_shift;
	// 	dst -= allign_shift & alignment;
	// 	allign_shift &= alignment - 1;
	// 
	// 	x_bound = std::min(this->m_locator.width, length);
	// 	row_base += this->m_locator.stride;
	// 	src = row_base;
	// 	length -= this->m_locator.width;
	// }
	// 
	// for (; x_bound < 0; ++x_bound) {
	// 	dst[x_bound] = 0;
	// }


	size_t row_shift = this->m_locator.width & (alignment - 1);	//pdu
	// need to compensate unalligned src block beginning, hence requirement for dst to be preceded
	// ptrdiff_t dst_shift = -((ptrdiff_t)(x & (alignment - 1)));		//pdu
	ptrdiff_t dst_shift = alignment - (x & (alignment - 1));	//pdu
	T* row_base = this->m_begin + y * this->m_locator.stride;
	T* src = row_base + (x & (~(alignment - 1)));
	T* eor = row_base + this->m_locator.width;
	T* last = row_base + ((x + length) / this->m_locator.width) * this->m_locator.stride + 
		(x + length) % this->m_locator.width;
	// T* tdst = dst;
	T* tdst = dst - (alignment & ((-dst_shift) >> ((sizeof(ptrdiff_t) << 3) - 1)));

	// reads src always alligned. writes dst always unalligned
	do {
		T* bound = std::min(eor, last);
		while (src < bound) {
			for (size_t i = 0; i < alignment; ++i) {
				tdst[i + dst_shift] = src[i];	//check signess of index type
			}
			src += alignment;
			tdst += alignment;
		}
		eor += this->m_locator.stride;
		src += this->m_locator.offset * 2;

		dst_shift += row_shift;
		// tdst -= dst_shift & alignment;
		tdst -= (~dst_shift) & alignment;
		dst_shift &= alignment - 1;
	} while (src < last);

	// accurate dst boundary handling
	for (T* eob = dst + length; eob < tdst; ++eob) {
		*eob = 0;
	}
	tdst = dst - alignment;
	for (size_t i = 0; i < alignment; ++i) {
		tdst[i] = 0;
	}
}

// template <typename T, size_t alignment>
// void bitmap<T, alignment>::linear(T* dst, size_t length, size_t linear_index) const noexcept {
// 	// Requires dst to be large enough to handle *length* elements + rest of alignment unit
// 	// at the end + should be preceded by alignment-size buffer. Otherwise blame yourself.
// 
// 	//size_t x = linear_index % this->m_locator.width;
// 	size_t y = linear_index / this->m_locator.width;
// 	// instead of modulo, all values are positive
// 	size_t x = linear_index - (y * this->m_locator.width);
// 
// 	size_t row_shift = this->m_locator.width & (alignment - 1);
// 	// need to compensate unalligned src block beginning, hence requirement for dst to be preceded
// 	ptrdiff_t dst_shift = -((ptrdiff_t)(x & (alignment - 1)));
// 	T* row_base = this->m_begin + y * this->m_locator.stride;
// 	T* src = row_base + (x & (~(alignment - 1)));
// 	T* eor = row_base + this->m_locator.width;
// 	T* last = row_base + ((x + length) / this->m_locator.width) * this->m_locator.stride +
// 		(x + length) % this->m_locator.width;
// 	T* tdst = dst;
// 
// 	// reads src always alligned. writes dst always unalligned
// 	do {
// 		T* bound = std::min(eor, last);
// 		while (src < bound) {
// 			for (size_t i = 0; i < alignment; ++i) {
// 				tdst[i + dst_shift] = src[i];
// 			}
// 			src += alignment;
// 			tdst += alignment;
// 		}
// 		eor += this->m_locator.stride;
// 		src += this->m_locator.offset * 2;
// 
// 		dst_shift += row_shift;
// 		tdst -= dst_shift & alignment;
// 		dst_shift &= alignment - 1;
// 	} while (src < last);
// 
// 	// accurate dst boundary handling
// 	for (T* eob = dst + length; eob < tdst; ++eob) {
// 		*eob = 0;
// 	}
// 	tdst = dst - alignment;
// 	for (size_t i = 0; i < alignment; ++i) {
// 		tdst[i] = 0;
// 	}
// }

// conversion-enabled implementation
template <typename T, size_t alignment>
template <typename D>
void bitmap<T, alignment>::linear(D* dst, size_t length, size_t linear_index) const noexcept {
	// Requires dst to be large enough to handle *length* elements + rest of alignment unit
	// at the end + should be preceded by alignment-size buffer. Otherwise blame yourself.

	//size_t x = linear_index % this->m_locator.width;
	size_t y = linear_index / this->m_locator.width;
	// instead of modulo, all values are positive
	size_t x = linear_index - (y * this->m_locator.width);

	size_t row_shift = this->m_locator.width & (alignment - 1);
	// need to compensate unalligned src block beginning, hence requirement for dst to be preceded
	ptrdiff_t dst_shift = -((ptrdiff_t)(x & (alignment - 1)));
	T* row_base = this->m_begin + y * this->m_locator.stride;
	T* src = row_base + (x & (~(alignment - 1)));
	T* eor = row_base + this->m_locator.width;
	T* last = row_base + ((x + length) / this->m_locator.width) * this->m_locator.stride +
		(x + length) % this->m_locator.width;
	D* tdst = dst;

	// reads src always alligned. writes dst always unalligned
	do {
		T* bound = std::min(eor, last);
		while (src < bound) {
			for (size_t i = 0; i < alignment; ++i) {
				tdst[i + dst_shift] = static_cast<D>(src[i]);
			}
			src += alignment;
			tdst += alignment;
		}
		eor += this->m_locator.stride;
		src += this->m_locator.offset * 2;

		dst_shift += row_shift;
		tdst -= dst_shift & alignment;
		dst_shift &= alignment - 1;
	} while (src < last);

	// accurate dst boundary handling
	for (T* eob = dst + length; eob < tdst; ++eob) {
		*eob = 0;
	}
	tdst = dst - alignment;
	for (size_t i = 0; i < alignment; ++i) {
		tdst[i] = 0;
	}
}

template <typename T, size_t alignment>
bitmap_row<T> bitmap<T, alignment>::operator[](size_t index) const {
	// TODO: check urvo and if std::move is needed
	return bitmap_row<T>(this->m_begin + this->m_locator.stride * index, this->m_locator.width);
}

template <typename T, size_t alignment>
bitmap<T, alignment>& bitmap<T, alignment>::operator<<=(size_t shift_amount) {
	for (size_t row = 0; row < this->m_locator.height; ++row) {
		// TODO: check if counting a pointer this way relaxes memory dependencies
		// as an alternative, v_dst += this->m_locator.stride; after the loop
		T* v_dst = this->m_begin + row * this->m_locator.stride;
		for (ptrdiff_t i = 0; i < this->m_locator.width; i += alignment) {
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				v_dst[i + j] <<= shift_amount;
			}
		}
	}
	return *this;
}

template <typename T, size_t alignment>
bitmap<T, alignment>& bitmap<T, alignment>::operator>>=(size_t shift_amount) {
	for (size_t row = 0; row < this->m_locator.height; ++row) {
		// TODO: check if counting a pointer this way relaxes memory dependencies
		// as an alternative, v_dst += this->m_locator.stride; after the loop
		T* v_dst = this->m_begin + row * this->m_locator.stride;
		for (ptrdiff_t i = 0; i < this->m_locator.width; i += alignment) {
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				v_dst[i + j] >>= shift_amount;
			}
		}
	}
	return *this;
}

template <typename T, size_t alignment>
img_meta bitmap<T, alignment>::get_meta() const {
	return this->m_locator;
}


template <typename T, size_t alignment>
void bitmap<T, alignment>::__reallocate() {
	delete[] this->m_rawdata;
	this->m_rawdata = new T[this->m_rawdata_length + (alignment - 1)];	// adds multiple of sizeof(T)
	// physical memory alignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->palignment - 1)) 
		& (~(this->palignment - 1)))) + this->m_locator.offset;
}

// friend swap function
template <typename T, size_t alignment>
void swap(bitmap<T, alignment>& first, bitmap<T, alignment>& second) noexcept {
	using std::swap;
	swap(first.m_begin, second.m_begin);
	swap(first.m_rawdata, second.m_rawdata);
	swap(first.m_locator, second.m_locator);
	swap(first.m_rawdata_length, second.rawdata_length);
}

namespace std {
	template <typename T, size_t alignment>
	void swap(bitmap<T, alignment>& first, bitmap<T, alignment>& second) noexcept {
		swap(first, second);
	}
}

// bitmap_row class template implementation

template <typename T, size_t alignment>
bitmap_row<T, alignment>::bitmap_row(T* data, size_t width) : row_start(data), row_width(width) {}

template <typename T, size_t alignment>
T& bitmap_row<T, alignment>::operator[](size_t index) const {
	return row_start[index];
}

template <typename T, size_t alignment>
bitmap_row<T, alignment>& bitmap_row<T, alignment>::assign(const bitmap_row& other) {
	size_t boundary = this->row_width + alignment; // TODO: check why not just width
	for (size_t i = 0; i < boundary; i += alignment) {
		for (size_t j = 0; j < alignment; ++j) {
			this->row_start[i + j] = other.row_start[i + j];
		}
	}
	return *this;
}

template <typename T, size_t alignment>
bitmap_row<T, alignment>& bitmap_row<T, alignment>::assign(T val) {
	size_t boundary = this->row_width + alignment;
	for (size_t i = 0; i < boundary; i += alignment) {
		for (size_t j = 0; j < alignment; ++j) {
			this->row_start[i + j] = val;
		}
	}
	return *this;
}

template <typename T, size_t alignment>
T* const bitmap_row<T, alignment>::ptr() const {
	return this->row_start;
}

template <typename T, size_t alignment>
size_t bitmap_row<T, alignment>::width() const {
	return this->row_width;
}


// non-member functions
template <typename T, size_t alignment>
void overlapBitmaps(const bitmap<T, alignment>& src, bitmap<T, alignment>& dst,
		size_t overlap_height) {
	//TODO: implement error handling
	[[unlikely]]
	if (src.get_meta().width != dst.get_meta().width) {
		// error
	}
	[[unlikely]]
	if ((src.get_meta().height < overlap_height) |
		(dst.get_meta().height < overlap_height)) {
		//error
	}

	ptrdiff_t src_offset_rownum = src.get_meta().height - overlap_height;
	for (ptrdiff_t i = 0; i < overlap_height; ++i) {
		dst[i].assign(src[src_offset_rownum + i]);
	}
}