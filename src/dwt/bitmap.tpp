#pragma once

#include <algorithm>
#include <vector>
#include <type_traits>
#include <cstddef>

#include "img_meta.hpp"

template <typename T, size_t alignment = 16>
class bitmap;

template <typename T, size_t alignment = 16>
class bitmap_slice;

template <typename T, size_t alignment = 16>
class const_bitmap_slice;

// TODO: we want bitmap_row of constant bitmap be able to modify out of range
// bitmap row data, i.e. the area of intermediate buffers between two adjacent 
// rows (that doesn't modify logic state of the bitmap instance)
// 
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

	T* const ptr() const; // TODO: what is the rationale to return *const? It can be casted to non-const pointer implicitly
	size_t width() const;
	T& operator[](size_t index) const;
private:
	friend class bitmap<T, alignment>;
	friend class bitmap_slice<T, alignment>;
	friend class const_bitmap_slice<T, alignment>;
	bitmap_row(T* data, size_t width);
};

template <typename T, size_t alignment>
void swap(bitmap<T, alignment>& first, bitmap<T, alignment>& second) noexcept;

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
	bitmap& operator=(const bitmap& other);

	bitmap(size_t offset = alignment);
	bitmap(size_t width, size_t height, T* origin);
	bitmap(size_t width, size_t height, size_t offset = 16);

	template <typename D, size_t a = alignment, typename = std::enable_if_t<!std::is_same_v<T, D>>>
	explicit operator bitmap<D, a>() const;

	// declaration order matters
	friend void swap<>(bitmap& first, bitmap& second) noexcept;
	void swap(bitmap& other) noexcept;

	void fill(size_t width, size_t height, T* origin);
	void append(const bitmap& src);
	void resize(size_t width, size_t height);
	void shrink(size_t height) noexcept;

	template <typename D = T, size_t a = alignment>
	bitmap<D, a> transpose() const;

	template <typename D>
	void linear(D* dst, size_t length, size_t linear_index) const noexcept;
	bitmap_slice<T, alignment> slice(img_pos pos);
	const_bitmap_slice<T, alignment> slice(img_pos pos) const;

	bitmap_row<T> operator[](size_t index) const;
	bitmap& operator<<=(size_t shift_amount);
	bitmap& operator>>=(size_t shift_amount);

	img_meta get_meta() const;
	img_pos single_frame_params() const;
private:
	void __reallocate();
};

template <typename T, size_t alignment>
void overlapBitmaps(const bitmap<T, alignment>& src, bitmap<T, alignment>& dst,
	size_t overlap_height);

template <typename T, size_t alignment>
class bitmap_slice {
	// turns out it's easier to access operators on object referenced by
	// std::reference_wrapper than by plain pointer
	std::reference_wrapper<bitmap<T, alignment>> src; 
	img_pos location;

public:
	bitmap_slice(bitmap<T, alignment>& src);

	bitmap_row<T, alignment> operator[](ptrdiff_t index);

	bitmap<T, alignment>& get_bitmap();
	const bitmap<T, alignment>& get_bitmap() const;
	const img_pos& get_description() const;

	// bitmap_slice& assign(const bitmap_slice& other);
	bitmap_slice& assign(const const_bitmap_slice<T, alignment>& other);
	bitmap_slice& assign(const bitmap<T, alignment>& other);

private:
	friend class bitmap<T, alignment>;
	bitmap_slice(bitmap<T, alignment>& src, img_pos pos);
};

template <typename T, size_t alignment>
class const_bitmap_slice {
	std::reference_wrapper<const bitmap<T, alignment>> src;
	img_pos location;

public:
	const_bitmap_slice(const bitmap_slice<T, alignment>& other);
	const_bitmap_slice(const bitmap<T, alignment>& src);
	const_bitmap_slice(const bitmap<T, alignment>&& src) = delete;

	bitmap_row<T, alignment> operator[](ptrdiff_t index) const;

	const bitmap<T, alignment>& get_bitmap() const;
	const img_pos& get_description() const;

	template <typename D = T, size_t a = alignment>
	bitmap<D, a> transpose() const;

private:
	friend class bitmap<T, alignment>;
	const_bitmap_slice(const bitmap<T, alignment>& src, img_pos pos);
};

// Implementations section
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
	T* temp = this->m_rawdata; // TODO: std::swap?
	this->m_rawdata = other.m_rawdata;
	other.m_rawdata = temp;

	this->m_begin = other.m_begin;
	this->m_locator = std::move(other.m_locator);
	this->m_rawdata_length = other.m_rawdata_length;

	return *this;
}

template <typename T, size_t alignment>
bitmap<T, alignment>& bitmap<T, alignment>::operator= (const bitmap<T, alignment>& other) {
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
bitmap<T, alignment>::bitmap(size_t offset) : m_rawdata(nullptr), m_begin(nullptr), m_rawdata_length(0) {
	offset += (offset == 0);
	offset = (offset + alignment - 1) & (~(alignment - 1));
	this->m_locator.offset = offset;
}

template <typename T, size_t alignment>
bitmap<T, alignment>::bitmap(size_t width, size_t height, size_t offset) {
	// need original width of the image to perform linear()
	this->m_locator.width = width;
	// that ensures every new row starts with aligned address
	width = (width + alignment - 1) & (~(alignment - 1));
	offset += (offset == 0);
	offset = (offset + alignment - 1) & (~(alignment - 1));
	this->m_locator.offset = offset;
	this->m_locator.stride = width + 2 * this->m_locator.offset;
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
template <typename D, size_t a, typename>
bitmap<T, alignment>::operator bitmap<D, a>() const {
	bitmap<D, a> result(this->m_locator.width, this->m_locator.height, this->m_locator.offset);
	for (size_t i = 0; i < this->m_locator.height; ++i) {
		for (size_t j = 0; j < this->m_locator.width; j += alignment) {
			for (size_t k = 0; k < alignment; ++k) {
				result[i][j + k] = static_cast<D>((*this)[i][j + k]);
			}
		}
	}

	return result;
}

template <typename T, size_t alignment>
void bitmap<T, alignment>::swap(bitmap<T, alignment>& other) noexcept {
	::template swap<T, alignment>(*this, other); // qualified lookup
}

template <typename T, size_t alignment>
void bitmap<T, alignment>::fill(size_t width, size_t height, T* origin) {
	this->m_locator.width = width;
	width = (width + alignment - 1) & (~(alignment - 1));
	this->m_locator.stride = width + 2 * this->m_locator.offset;
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
void bitmap<T, alignment>::append(const bitmap<T, alignment>& src) {
	// short-circuit for non-owning this (e.g., default-initialized)
	// TODO: validate invariant against such scenarios
	if (this->m_rawdata == nullptr) {
		(*this) = src;
		return;
	}

	bool valid = true;
	valid &= src.m_locator.width == this->m_locator.width;
	if (!valid) {
		// TODO: handle error
	}

	bitmap result(this->m_locator.width, this->m_locator.height + src.m_locator.height, this->m_locator.offset);
	result.slice(this->single_frame_params()).assign(*this);
	img_pos frame = src.single_frame_params();
	frame.y += this->m_locator.height;
	result.slice(frame).assign(src);
	this->swap(result);
}

template <typename T, size_t alignment>
void bitmap<T, alignment>::resize(size_t width, size_t height) {
	bool subframe = (width <= this->m_locator.width) & (height <=  this->m_locator.height);
	if (subframe) {
		// change only logical dimensions, don't change layout parameters. This 
		// preserves the current bitmap data, involves no allocations
		// 
		// but what about offset?
		// TODO: that invalidates assumptions for this->linear; current implementation relies on 
		// double offset distance (adjusted for paddings) between subsequent rows
		//
		this->m_locator.width = width;
		this->m_locator.height = height;
		return;
	}

	this->m_locator.width = width;
	width = (width + alignment - 1) & (~(alignment - 1));
	this->m_locator.stride = width + 2 * this->m_locator.offset;
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
template <typename D, size_t a>
bitmap<D, a> bitmap<T, alignment>::transpose() const {
	// bitmap<D, a> transposed(this->m_locator.height, this->m_locator.width, this->m_locator.offset);
	// 
	// ptrdiff_t y_bound = this->m_locator.height - alignment;
	// size_t i = 0;
	// for (; i < y_bound; i += alignment) {
	// 	for (size_t k = 0; k < this->m_locator.width; ++k) {
	// 		bitmap_row<D> dstrow = transposed[k];
	// 		for (size_t j = 0; j < alignment; ++j) {
	// 			dstrow[i + j] = (*this)[i + j][k];
	// 		}
	// 	}
	// }
	// 
	// for (; i < this->m_locator.height; ++i) {
	// 	for (size_t k = 0; k < this->m_locator.width; ++k) {
	// 		transposed[k][i] = (*this)[i][k];
	// 	}
	// }
	// 
	// return transposed;
	return this->slice(this->single_frame_params()).template transpose<D, a>();
}

template <typename T, size_t alignment>
template <typename D>
void bitmap<T, alignment>::linear(D* dst, size_t length, size_t linear_index) const noexcept {
	// Requires dst to be large enough to handle *length* elements + rest of alignment unit
	// at the end + should be preceded by alignment-size buffer. Otherwise blame yourself.

	//size_t x = linear_index % this->m_locator.width;
	ptrdiff_t y = linear_index / this->m_locator.width;
	// instead of modulo, all values are non-negative
	// TODO: but both coordinates are result for a single idiv operation on common platforms...
	ptrdiff_t x = linear_index - (y * this->m_locator.width);

	ptrdiff_t row_shift = (-(ptrdiff_t)(this->m_locator.width)) & (alignment - 1);
	// need to compensate unaligned src block beginning, hence requirement for dst to be preceded
	// ptrdiff_t dst_shift = -((ptrdiff_t)(x & (alignment - 1)));
	ptrdiff_t dst_shift = x & (alignment - 1);
	// ptrdiff_t dst_shift = ((-x) & (alignment - 1));
	T* row_base = this->m_begin + y * this->m_locator.stride;
	T* src = row_base + (x & (~(alignment - 1)));
	T* eor = row_base + this->m_locator.width;
	T* last = row_base + ((x + length) / this->m_locator.width) * this->m_locator.stride +
		(x + length) % this->m_locator.width;
	D* tdst = dst - dst_shift;

	// reads src always aligned. writes dst always unaligned
	do {
		T* bound = std::min(eor, last);
		while (src < bound) {
			for (ptrdiff_t i = 0; i < alignment; ++i) {
				tdst[i] = static_cast<D>(src[i]);
			}
			src += alignment;
			tdst += alignment;
		}
		eor += this->m_locator.stride;
		src += this->m_locator.offset * 2;

		tdst -= row_shift;
	} while (src < last);
	// tdst += row_shift;

	// accurate dst boundary handling
	size_t aligned_length = (length + (alignment - 1)) & (~(alignment - 1));
	// D* eob = dst + aligned_length;
	// for (tdst = dst + length; tdst < eob; ++tdst) {
	// 	*tdst = 0;
	// }
	for (ptrdiff_t i = length; i < aligned_length; ++i) {
		dst[i] = 0;
	}
	tdst = dst - alignment;
	for (ptrdiff_t i = 0; i < alignment; ++i) {
		tdst[i] = 0;
	}

	//					~		-
	// 0x01 -> 0x0f		0x0e	0xff
	// 0x04 -> 0x0c		0x0b	0xfc
	// 0x07 -> 0x09
	// 0x0b -> 0x05		0x04	0xf5
	//
}

template <typename T, size_t alignment>
bitmap_slice<T, alignment> bitmap<T, alignment>::slice(img_pos pos) {
	bool valid = true;
	valid &= pos.x + pos.width <= this->m_locator.width;
	valid &= pos.y + pos.height <= this->m_locator.height;
	if (!valid) {
		// TODO: validation error
	}

	pos.x_stride = m_locator.stride; // TODO: set other positioning data?
	pos.depth = m_locator.depth;
	return bitmap_slice<T, alignment>(*this, pos);
}

template <typename T, size_t alignment>
const_bitmap_slice<T, alignment> bitmap<T, alignment>::slice(img_pos pos) const {
	bool valid = true;
	valid &= pos.x + pos.width <= this->m_locator.width;
	valid &= pos.y + pos.height <= this->m_locator.height;
	if (!valid) {
		// TODO: validation error
	}

	pos.x_stride = m_locator.stride;
	pos.depth = m_locator.depth;
	return const_bitmap_slice<T, alignment>(*this, pos);
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
		// TODO: weak element indexing using this->m_locator.length instead of 
		// [row + column]
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
	// TODO: either SFINAE that enables for integer T only, either specialization 
	// that performs similar transformation by increasing exponent value for fp T
	// TODO: but fp instantiations are compiled by msvc with no errors... huh?
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
img_pos bitmap<T, alignment>::single_frame_params() const {
	return {
		0, 0,
		0, 0,
		0, 0,
		this->m_locator.stride,
		this->m_locator.width,
		this->m_locator.length, // because depth is assumed to equal 1
		this->m_locator.height,
		this->m_locator.length,
		this->m_locator.depth
	};
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
	swap(first.m_rawdata_length, second.m_rawdata_length);
}

// C++20 prohibits providing specialization for function templates in std namespace
// namespace std {
// 	template <typename T, size_t alignment>
// 	void swap(bitmap<T, alignment>& first, bitmap<T, alignment>& second) noexcept {
// 		swap(first, second);
// 	}
// }


// bitmap_row class template implementation

template <typename T, size_t alignment>
bitmap_row<T, alignment>::bitmap_row(T* data, size_t width) : row_start(data), row_width(width) {}

template <typename T, size_t alignment>
T& bitmap_row<T, alignment>::operator[](size_t index) const {
	return row_start[index];
}

template <typename T, size_t alignment>
bitmap_row<T, alignment>& bitmap_row<T, alignment>::assign(const bitmap_row& other) {
	bool valid = true;
	valid &= other.row_width == this->row_width; // or less than?
	if (!valid) {
		// TODO: validation error (performance ?)
	}

	// need precise border handling because bitmap_row is used in bitmap_slice
	// TODO: but several redundant moves if length is multiple of alignment
	std::array<T, alignment> tail_buffer;
	// ptrdiff_t tail_start = (this->row_width + (alignment - 1)) & (~(alignment - 1));
	ptrdiff_t tail_start = this->row_width & (~(alignment - 1));
	for (ptrdiff_t i = 0; i < alignment; ++i) {
		tail_buffer[i] = this->row_start[tail_start + i];
	}

	for (ptrdiff_t i = 0; i < tail_start; i += alignment) {
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			this->row_start[i + j] = other.row_start[i + j];
		}
	}

	std::array<T, alignment> other_tail_buffer;
	for (ptrdiff_t i = 0; i < alignment; ++i) {
		other_tail_buffer[i] = other.row_start[tail_start + i];
	}

	ptrdiff_t precise_bound = this->row_width & (alignment - 1);
	for (ptrdiff_t i = 0; i < precise_bound; ++i) {
		tail_buffer[i] = other_tail_buffer[i];
	}

	for (ptrdiff_t i = 0; i < alignment; ++i) {
		this->row_start[tail_start + i] = tail_buffer[i];
	}

	return *this;
}

template <typename T, size_t alignment>
bitmap_row<T, alignment>& bitmap_row<T, alignment>::assign(T val) {
	// size_t boundary = this->row_width + alignment;
	// for (size_t i = 0; i < boundary; i += alignment) {
	// 	for (size_t j = 0; j < alignment; ++j) {
	// 		this->row_start[i + j] = val;
	// 	}
	// }

	std::array<T, alignment> tail_buffer;
	ptrdiff_t tail_start = (this->row_width + (alignment - 1)) & (~(alignment - 1));
	for (ptrdiff_t i = 0; i < alignment; ++i) {
		tail_buffer[i] = this->row_start[tail_start + i];
	}

	for (ptrdiff_t i = 0; i < tail_start; i += alignment) {
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			this->row_start[i + j] = val;
		}
	}

	ptrdiff_t precise_bound = this->row_width & (alignment - 1);
	for (ptrdiff_t i = 0; i < precise_bound; ++i) {
		tail_buffer[i] = val;
	}

	for (ptrdiff_t i = 0; i < alignment; ++i) {
		this->row_start[tail_start + i] = tail_buffer[i];
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


// bitmap_slice class template implementation

template <typename T, size_t alignment>
bitmap_slice<T, alignment>::bitmap_slice(bitmap<T, alignment>& src) : 
	bitmap_slice(src, src.single_frame_params()) {}

template <typename T, size_t alignment>
bitmap_slice<T, alignment>::bitmap_slice(bitmap<T, alignment>& src, img_pos location) 
	: src(src), location(location) {}

template <typename T, size_t alignment>
bitmap_row<T, alignment> bitmap_slice<T, alignment>::operator[](ptrdiff_t index) {
	bool valid = true;
	valid &= index < this->location.height;
	if (!valid) {
		// TODO: validation error
		// but use of subscription operator is expected to be heavy, maybe boundary 
		// checks are not desirable.
		// On the other hand, having validation branch marked as [[unlikely]], the 
		// overhead is induced by trivial intergral/bool operations and is expected 
		// to be miserable (?). We perform some arithmetic to compute necessary index 
		// anyway, compute resources needed for payload are heavier
		// 
		// But we'd like noexcept specification on the operator. It's not clear how to 
		// handle anyway
		//
	}
	return bitmap_row<T, alignment>(this->src.get()[this->location.y + index].ptr() + this->location.x, this->location.width);
}

template <typename T, size_t alignment>
bitmap<T, alignment>& bitmap_slice<T, alignment>::get_bitmap() {
	return this->src;
}

template <typename T, size_t alignment>
const bitmap<T, alignment>& bitmap_slice<T, alignment>::get_bitmap() const {
	return this->src;
}

template <typename T, size_t alignment>
const img_pos& bitmap_slice<T, alignment>::get_description() const {
	return this->location;
}

// template <typename T, size_t alignment>
// bitmap_slice<T, alignment>& bitmap_slice<T, alignment>::assign(const bitmap_slice& other) {
// 	bool valid = true;
// 	valid &= other.location.width == this->location.width;
// 	valid &= other.location.height == this->location.height;
// 	if (!valid) {
// 		// TODO: validation error
// 	}
// 	
// 	for (ptrdiff_t i = 0; i < this->location.height; ++i) {
// 		(*this)[i].assign(other[i]);
// 	}
// }

template <typename T, size_t alignment>
bitmap_slice<T, alignment>& bitmap_slice<T, alignment>::assign(const const_bitmap_slice<T, alignment>& other) {
	bool valid = true;
	const img_pos& other_dims = other.get_description();
	valid &= other_dims.width == this->location.width;
	valid &= other_dims.height == this->location.height;
	if (!valid) {
		// TODO: validation error
	}

	for (ptrdiff_t i = 0; i < this->location.height; ++i) {
		(*this)[i].assign(other[i]);
	}

	return *this;
}

template <typename T, size_t alignment>
bitmap_slice<T, alignment>& bitmap_slice<T, alignment>::assign(const bitmap<T, alignment>& other) {
	return this->assign(other.slice(other.single_frame_params()));
}


// const_bitmap_slice class template implementation

template <typename T, size_t alignment>
const_bitmap_slice<T, alignment>::const_bitmap_slice(const bitmap<T, alignment>& src) : 
	const_bitmap_slice(src, src.single_frame_params()) {}

template <typename T, size_t alignment>
const_bitmap_slice<T, alignment>::const_bitmap_slice(const bitmap<T, alignment>& src, img_pos location) 
	: src(src), location(location) {}

template <typename T, size_t alignment>
const_bitmap_slice<T, alignment>::const_bitmap_slice(const bitmap_slice<T, alignment>& other) 
		: src(other.get_bitmap()), location(other.get_description()) {}

template <typename T, size_t alignment>
bitmap_row<T, alignment> const_bitmap_slice<T, alignment>::operator[](ptrdiff_t index) const {
	bool valid = true;
	valid &= index < this->location.height;
	if (valid) {
		// TODO: validation error
	}

	return bitmap_row<T, alignment>(this->src.get()[this->location.y + index].ptr() + this->location.x, this->location.width);
}

template <typename T, size_t alignment>
const bitmap<T, alignment>& const_bitmap_slice<T, alignment>::get_bitmap() const {
	return this->src;
}

template <typename T, size_t alignment>
const img_pos& const_bitmap_slice<T, alignment>::get_description() const {
	return this->location;
}

template <typename T, size_t alignment>
template <typename D, size_t a>
bitmap<D, a> const_bitmap_slice<T, alignment>::transpose() const {
	img_meta src_meta = src.get().get_meta();
	bitmap<D, a> transposed(this->location.height, this->location.width, src_meta.offset);
	img_meta dst_meta = transposed.get_meta();

	const T* src = this->src.get()[this->location.y].ptr() + this->location.x;
	std::array<std::array<T, alignment>, alignment> transpose_buffer = {};
	for (ptrdiff_t i = 0; i < this->location.height; i += alignment) {
		ptrdiff_t max_src_row_index = std::min<ptrdiff_t>(this->location.height - i, alignment);
		for (ptrdiff_t j = 0; j < dst_meta.height; j += alignment) {
			ptrdiff_t max_dst_row_index = std::min<ptrdiff_t>(dst_meta.height - j, alignment);
			for (ptrdiff_t ii = 0; ii < max_src_row_index; ++ii) {
				const T* src_row = src + ((i + ii) * this->location.x_stride); // TODO: x_step?
				for (ptrdiff_t jj = 0; jj < alignment; ++jj) {
					transpose_buffer[jj][ii] = src_row[j + jj];
				}
			}
			for (ptrdiff_t ii = 0; ii < max_dst_row_index; ++ii) {
				bitmap_row<D, a> dstrow = transposed[j + ii];
				for (ptrdiff_t jj = 0; jj < alignment; ++jj) {
					dstrow[i + jj] = static_cast<D>(transpose_buffer[ii][jj]);
				}
			}
		}
	}

	return transposed;
}


// non-member functions
template <typename T, size_t alignment>
void overlapBitmaps(const bitmap<T, alignment>& src, bitmap<T, alignment>& dst,
		size_t overlap_height) {
	bool valid = true;
	valid &= src.get_meta().height > overlap_height;
	valid &= dst.get_meta().height > overlap_height;
	valid &= src.get_meta().width == dst.get_meta().width;

	if (!valid) {
		//TODO: implement error handling
	}

	img_pos src_frame = src.single_frame_params();
	img_pos dst_frame = dst.single_frame_params();

	src_frame.y = src_frame.height - overlap_height;
	src_frame.height = overlap_height;
	dst_frame.height = overlap_height;

	dst.slice(dst_frame).assign(src.slice(src_frame));

	// ptrdiff_t src_offset_rownum = src.get_meta().height - overlap_height;
	// for (ptrdiff_t i = 0; i < overlap_height; ++i) {
	// 	dst[i].assign(src[src_offset_rownum + i]);
	// }
}