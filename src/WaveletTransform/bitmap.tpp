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

template <typename T, size_t allignment = 16>
class bitmap;

template <typename T, size_t allignment = 16>
class bitmap_row {
	T* const row_start;
	size_t row_width;
public:
	// TODO: this may break language built-in mechanisms
	// // Warning!
	// // Move only, but copy assignemnt is used for an aggregate operation
	bitmap_row(bitmap_row&& other) = default;
	bitmap_row& operator=(bitmap_row&& other) = default;

	bitmap_row(const bitmap_row&) = delete;
	bitmap_row& operator=(const bitmap_row& other) = delete;;
	// // inaccurate assign operator, vectorizable
	// bitmap_row& operator=(const bitmap_row& other);
	// // Fill assignement
	// bitmap_row& operator=(T other);


	// inaccurate content assignment, vectorizable
	bitmap_row& assign(const bitmap_row& source);
	// Fill content assignement
	bitmap_row& assign(T val);

	T* const ptr() const;
	size_t width() const;
	T& operator[](size_t index) const;
private:
	friend class bitmap<T, allignment>;
	bitmap_row(T* data, size_t width);
};

template <typename T, size_t allignment>
class bitmap {
	T* m_begin;
	T* m_rawdata;
	img_meta m_locator;
	size_t m_rawdata_length;

	static constexpr size_t pallignment = allignment * sizeof(T);
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
	void linear(T* dst, size_t length, size_t linear_index) const noexcept;
	bitmap_row<T> operator[](size_t index) const;

	img_meta getImgMeta() const;
private:
	void __reallocate();
};

template <typename T, size_t allignment>
bitmap<T, allignment>::~bitmap() {
	delete[] this->m_rawdata;
}

template <typename T, size_t allignment>
bitmap<T, allignment>::bitmap(bitmap&& other) noexcept :
		m_begin(other.m_begin), m_rawdata(other.m_rawdata),
		m_locator(std::move(other.m_locator)), m_rawdata_length(other.m_rawdata_length) {
	other.m_rawdata = nullptr;
	other.m_begin = nullptr;
	other.m_rawdata_length = 0;
}

template <typename T, size_t allignment>
bitmap<T, allignment>::bitmap(const bitmap& other) : m_locator(other.m_locator) {
	this->m_rawdata_length = std::min(other.m_locator.length + other.m_locator.offset, 
		other.m_rawdata_length);

	this->m_rawdata = new T[this->m_rawdata_length + (allignment - 1)];	// adds multiple of sizeof(T)
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->pallignment - 1)) 
		& (~(this->pallignment - 1)))) + this->m_locator.offset;

	for (size_t i = 0; i < this->m_locator.length; i += allignment) {
		for (size_t j = 0; j < allignment; ++j) {
			this->m_begin[i + j] = other.m_begin[i + j];
		}
	}
}

template <typename T, size_t allignment>
bitmap<T, allignment>& bitmap<T, allignment>::operator= (bitmap<T, allignment>&& other) noexcept {
	T* temp = this->m_rawdata;
	this->m_rawdata = other.m_rawdata;
	other.m_rawdata = temp;

	this->m_begin = other.m_begin;
	this->m_locator = std::move(other.m_locator);
	this->m_rawdata_length = other.m_rawdata_length; // + other.m_locator.offset;

	// other.m_rawdata = nullptr; // TODO: potential memory leak?
	return *this;
}

template <typename T, size_t allignment>
bitmap<T, allignment>& bitmap<T, allignment>::operator= (bitmap<T, allignment>& other) {
	if (this != &other) {
		this->m_locator = other.m_locator;

		if (this->m_rawdata_length < (other.m_locator.length + other.m_locator.offset)) {
			this->m_rawdata_length = std::min(other.m_locator.length + other.m_locator.offset, 
				other.m_rawdata_length);
			this->__reallocate();
		}

		for (size_t i = 0; i < this->m_locator.length; i += allignment) {
			for (size_t j = 0; j < allignment; ++j) {
				this->m_begin[i + j] = other.m_begin[i + j];
			}
		}
	}
	return *this;
}

template <typename T, size_t allignment>
bitmap<T, allignment>::bitmap() : m_rawdata(nullptr), m_begin(nullptr), m_rawdata_length(0) {}

template <typename T, size_t allignment>
bitmap<T, allignment>::bitmap(size_t width, size_t height, T* origin) {
	this->m_locator.width = width;
	width = (width + allignment - 1) & (~(allignment - 1));		//pdu - per data unit
	this->m_locator.offset = allignment; 	//pdu
	this->m_locator.stride = width + 2 * allignment; 	//pdu
	this->m_locator.height = height;
	this->m_locator.depth = 1;
	this->m_locator.length = this->m_locator.height * this->m_locator.stride; 	//pdu
	this->m_rawdata_length = this->m_locator.length + this->m_locator.offset;	//pdu

	this->m_rawdata = new T[this->m_rawdata_length + (allignment - 1)];	// adds multiple of sizeof(T)
	// physical memory allignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->pallignment - 1)) 
		& (~(this->pallignment - 1)))) + this->m_locator.offset;	//((T*)ppb) + pdu - per physical bytes

	for (size_t row = 0; row < height; ++row) {
		// this->m_locator.width stores original width value
		T* v_src = origin + row * this->m_locator.width;
		T* v_dst = this->m_begin + row * this->m_locator.stride;
		size_t i;
		for (i = 0; i < /*v_bound*/ this->m_locator.width; i += allignment) {
			v_dst[i] = v_src[i];
		}
	}
}

template <typename T, size_t allignment>
bitmap<T, allignment>::bitmap(size_t width, size_t height, size_t offset) {
	// need original width of the image to perform linear()
	this->m_locator.width = width;
	// that ensures every new row starts with alligned address
	width = (width + allignment - 1) & (~(allignment - 1));
	offset = (offset + allignment) & (~(allignment - 1));
	this->m_locator.offset = offset;
	this->m_locator.stride = width + 2 * offset;
	this->m_locator.height = height;
	this->m_locator.depth = 1;
	this->m_locator.length = this->m_locator.height * this->m_locator.stride;
	this->m_rawdata_length = this->m_locator.length + this->m_locator.offset;

	this->m_rawdata = new T[this->m_rawdata_length + (allignment - 1)];	// adds multiple of sizeof(T)
	// physical memory allignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->pallignment - 1)) 
		& (~(this->pallignment - 1)))) + this->m_locator.offset;
}

template <typename T, size_t allignment>
void bitmap<T, allignment>::swap(bitmap<T, allignment>& other) noexcept {
	swap(*this, other);
}

template <typename T, size_t allignment>
void bitmap<T, allignment>::fill(size_t width, size_t height, T* origin) {
	this->m_locator.width = width;
	width = (width + allignment - 1) & (~(allignment - 1));
	this->m_locator.offset = allignment;
	this->m_locator.stride = width + 2 * allignment;
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
		for (size_t i = 0; i < this->m_locator.width; i += allignment) {
			v_dst[i] = v_src[i];
		}
	}
}

template <typename T, size_t allignment>
void bitmap<T, allignment>::resize(size_t width, size_t height) {
	this->m_locator.width = width;
	width = (width + allignment - 1) & (~(allignment - 1));
	this->m_locator.offset = allignment;
	this->m_locator.stride = width + 2 * allignment;
	this->m_locator.height = height;
	this->m_locator.depth = 1;
	this->m_locator.length = this->m_locator.height * this->m_locator.stride;

	if (this->m_rawdata_length < (this->m_locator.length + this->m_locator.offset)) {
		this->m_rawdata_length = this->m_locator.length + this->m_locator.offset;
		this->__reallocate();
	}
}

template <typename T, size_t allignment>
void bitmap<T, allignment>::shrink(size_t height) noexcept {
	this->m_locator.height = height;
}

template <typename T, size_t allignment>
bitmap<T> bitmap<T, allignment>::transpose() const {
	bitmap<T> transposed(this->m_locator.height, this->m_locator.width, this->m_locator.offset);
	for (size_t i = 0; i < this->m_locator.height; i += allignment) {
		for (size_t k = 0; k < this->m_locator.width; ++k) {
			bitmap_row<T> dstrow = transposed[k];
			for (size_t j = 0; j < allignment; ++j) {
				dstrow[i + j] = (*this)[i + j][k];
			}
		}
	}
	return transposed;
}

template <typename T, size_t allignment>
void bitmap<T, allignment>::linear(T* dst, size_t length, size_t x, size_t y) const noexcept {
	// Requires dst to be large enough to handle *length* elements + rest of allignment unit
	// at the end + should be preceded by allignment-size buffer. Otherwise blame yourself.
	
	// size_t row_shift = this->m_locator.width & (allignment - 1);
	// size_t allign_shift = (y * row_shift + x) & (allignment - 1);
	// size_t row_count = (length + x) / this->m_locator.width;
	// 
	// size_t x_bound = std::min(this->m_locator.width - x, length);
	// T* row_base = this->m_begin + y * this->m_locator.stride;
	// T* src = row_base + x;
	// for (; row_count >= 0; --row_count) {
	// 	for (; x_bound > 0; x_bound -= allignment, dst += allignment, src += allignment) {
	// 		for (size_t i = 0; i < allignment; ++i) {
	// 			dst[allign_shift + i] = src[i];
	// 		}
	// 	}
	// 
	// 	allign_shift += row_shift;
	// 	dst -= allign_shift & allignment;
	// 	allign_shift &= allignment - 1;
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


	size_t row_shift = this->m_locator.width & (allignment - 1);	//pdu
	// need to compensate unalligned src block beginning, hence requirement for dst to be preceded
	// ptrdiff_t dst_shift = -((ptrdiff_t)(x & (allignment - 1)));		//pdu
	ptrdiff_t dst_shift = allignment - (x & (allignment - 1));	//pdu
	T* row_base = this->m_begin + y * this->m_locator.stride;
	T* src = row_base + (x & (~(allignment - 1)));
	T* eor = row_base + this->m_locator.width;
	T* last = row_base + ((x + length) / this->m_locator.width) * this->m_locator.stride + 
		(x + length) % this->m_locator.width;
	// T* tdst = dst;
	T* tdst = dst - (allignment & ((-dst_shift) >> ((sizeof(ptrdiff_t) << 3) - 1)));

	// reads src always alligned. writes dst always unalligned
	do {
		T* bound = std::min(eor, last);
		while (src < bound) {
			for (size_t i = 0; i < allignment; ++i) {
				tdst[i + dst_shift] = src[i];	//check signess of index type
			}
			src += allignment;
			tdst += allignment;
		}
		eor += this->m_locator.stride;
		src += this->m_locator.offset * 2;

		dst_shift += row_shift;
		// tdst -= dst_shift & allignment;
		tdst -= (~dst_shift) & allignment;
		dst_shift &= allignment - 1;
	} while (src < last);

	// accurate dst boundary handling
	for (T* eob = dst + length; eob < tdst; ++eob) {
		*eob = 0;
	}
	tdst = dst - allignment;
	for (size_t i = 0; i < allignment; ++i) {
		tdst[i] = 0;
	}
}

template <typename T, size_t allignment>
void bitmap<T, allignment>::linear(T* dst, size_t length, size_t linear_index) const noexcept {
	// Requires dst to be large enough to handle *length* elements + rest of allignment unit
	// at the end + should be preceded by allignment-size buffer. Otherwise blame yourself.

	//size_t x = linear_index % this->m_locator.width;
	size_t y = linear_index / this->m_locator.width;
	// instead of modulo, all values are positive
	size_t x = linear_index - (y * this->m_locator.width);

	size_t row_shift = this->m_locator.width & (allignment - 1);
	// need to compensate unalligned src block beginning, hence requirement for dst to be preceded
	ptrdiff_t dst_shift = -((ptrdiff_t)(x & (allignment - 1)));
	T* row_base = this->m_begin + y * this->m_locator.stride;
	T* src = row_base + (x & (~(allignment - 1)));
	T* eor = row_base + this->m_locator.width;
	T* last = row_base + ((x + length) / this->m_locator.width) * this->m_locator.stride +
		(x + length) % this->m_locator.width;
	T* tdst = dst;

	// reads src always alligned. writes dst always unalligned
	do {
		T* bound = std::min(eor, last);
		while (src < bound) {
			for (size_t i = 0; i < allignment; ++i) {
				tdst[i + dst_shift] = src[i];
			}
			src += allignment;
			tdst += allignment;
		}
		eor += this->m_locator.stride;
		src += this->m_locator.offset * 2;

		dst_shift += row_shift;
		tdst -= dst_shift & allignment;
		dst_shift &= allignment - 1;
	} while (src < last);

	// accurate dst boundary handling
	for (T* eob = dst + length; eob < tdst; ++eob) {
		*eob = 0;
	}
	tdst = dst - allignment;
	for (size_t i = 0; i < allignment; ++i) {
		tdst[i] = 0;
	}
}

template <typename T, size_t allignment>
bitmap_row<T> bitmap<T, allignment>::operator[](size_t index) const {
	// TODO: check urvo and if std::move is needed
	return bitmap_row<T>(this->m_begin + this->m_locator.stride * index, this->m_locator.width);
}

template <typename T, size_t allignment>
img_meta bitmap<T, allignment>::getImgMeta() const {
	return this->m_locator;
}


template <typename T, size_t allignment>
void bitmap<T, allignment>::__reallocate() {
	delete[] this->m_rawdata;
	this->m_rawdata = new T[this->m_rawdata_length + (allignment - 1)];	// adds multiple of sizeof(T)
	// physical memory allignment
	this->m_begin = ((T*)(((size_t)this->m_rawdata + (this->pallignment - 1)) 
		& (~(this->pallignment - 1)))) + this->m_locator.offset;
}

// friend swap function
template <typename T, size_t allignment>
void swap(bitmap<T, allignment>& first, bitmap<T, allignment>& second) noexcept {
	using std::swap;
	swap(first.m_begin, second.m_begin);
	swap(first.m_rawdata, second.m_rawdata);
	swap(first.m_locator, second.m_locator);
	swap(first.m_rawdata_length, second.rawdata_length);
}

namespace std {
	template <typename T, size_t allignment>
	void swap(bitmap<T, allignment>& first, bitmap<T, allignment>& second) noexcept {
		swap(first, second);
	}
}

template <typename T, size_t allignment>
bitmap_row<T, allignment>::bitmap_row(T* data, size_t width) : row_start(data), row_width(width) {}

template <typename T, size_t allignment>
T& bitmap_row<T, allignment>::operator[](size_t index) const {
	return row_start[index];
}

template <typename T, size_t allignment>
bitmap_row<T, allignment>& bitmap_row<T, allignment>::assign(const bitmap_row& other) {
	size_t boundary = this->row_width + allignment; // TODO: check why not just width
	for (size_t i = 0; i < boundary; i += allignment) {
		for (size_t j = 0; j < allignment; ++j) {
			this->row_start[i + j] = other.row_start[i + j];
		}
	}
	return *this;
}

template <typename T, size_t allignment>
bitmap_row<T, allignment>& bitmap_row<T, allignment>::assign(T val) {
	size_t boundary = this->row_width + allignment;
	for (size_t i = 0; i < boundary; i += allignment) {
		for (size_t j = 0; j < allignment; ++j) {
			this->row_start[i + j] = val;
		}
	}
	return *this;
}

template <typename T, size_t allignment>
T* const bitmap_row<T, allignment>::ptr() const {
	return this->row_start;
}

template <typename T, size_t allignment>
size_t bitmap_row<T, allignment>::width() const {
	return this->row_width;
}

// non-member functions
template <typename T, size_t allignment>
void overlapBitmaps(const bitmap<T, allignment>& src, bitmap<T, allignment>& dst,
		size_t overlap_height) {
	//TODO: implement error handling
	[[unlikely]]
	if (src.getImgMeta().width != dst.getImgMeta().width) {
		// error
	}
	[[unlikely]]
	if ((src.getImgMeta().height < overlap_height) |
		(dst.getImgMeta().height < overlap_height)) {
		//error
	}

	ptrdiff_t src_offset_rownum = src.getImgMeta().height - overlap_height;
	for (ptrdiff_t i = 0; i < overlap_height; ++i) {
		dst[i].assign(src[src_offset_rownum + i]);
	}
}