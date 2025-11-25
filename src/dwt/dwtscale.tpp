#pragma once

#include <array>

#include "bitmap.tpp"

// During dwt, only optional subband shifts are performrmed as part 
// of subband scaling. The other part of scaling (that is type 
// conversion) is handled during segment processing.
// 
// bit_shifts collection is necessary for both int and fp transforms, 
// because shift amount values for different subbands are used during 
// bpe.
//

template <typename T>
struct _dwtscale_i {
	shifts_t bit_shifts{ 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };

public:
	template <size_t alignment>
	void scale(bitmap<T, alignment>& subband, ptrdiff_t index) {
		subband <<= this->bit_shifts[index];
	}

	template <size_t alignment>
	void unscale(bitmap<T, alignment>& subband, ptrdiff_t index) {
		subband >>= this->bit_shifts[index];
	}

	void set_shifts(const shifts_t& shifts) {
		this->bit_shifts = shifts;
	}

	shifts_t get_shifts() const {
		return this->bit_shifts;
	}
};

template <typename T>
struct _dwtscale_f {
	shifts_t bit_shifts{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

public:
	template <size_t alignment>
	void scale(bitmap<T, alignment>& subband, ptrdiff_t index) { }

	template <size_t alignment>
	void unscale(bitmap<T, alignment>& subband, ptrdiff_t index) { }

	shifts_t get_shifts() const {
		return this->bit_shifts;
	}
};


template <typename T, bool is_int>
struct _dwtscale;

template <typename T>
struct _dwtscale<T, true> {
	using type = typename _dwtscale_i<T>;
};

template <typename T>
struct _dwtscale<T, false> {
	using type = typename _dwtscale_f<T>;
};

template <typename T>
using dwtscale = _dwtscale<T, std::is_integral_v<T>>::type;
