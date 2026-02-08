#pragma once

#include <array>
#include <deque>
#include <bit>
#include <functional>

#include "utils.hpp"
#include "entropy_types.hpp"
#include "obitwrapper.tpp"
#include "ibitwrapper.tpp"

// TODO: should return _vlw_t<T>?
template <typename T>
inline vlw_t combine_bword(T raw, T select_mask);

// TODO: should take _vlw_t<T> as parameter?
template <typename T>
inline T decompose_bword(T packed, T unpack_mask);

template <typename T, typename obwT, size_t alignment = 16, bool omit_left_shifts = false>
void bplane(T* data, size_t size, size_t b, obitwrapper<obwT>& ostream, 
		std::function<void(T&)> transform = [](T&) -> void {});

template <size_t b, typename T, typename obwT, size_t alignment = 16>
void bplane_static(T* data, size_t size, obitwrapper<obwT>& ostream, 
		std::function<void(std::type_identity_t<T>&)> transform = [](T&) -> void {});

template <typename T, typename obwT, size_t alignment = 16>
void bplanev4(T* data, size_t size, size_t belder, std::array<std::reference_wrapper<obitwrapper<obwT>>, 4> ostreams);

template <typename T, typename obwT, size_t alignment = 16>
void bplaneEncode(T* data, size_t datasize, size_t pindex, size_t pcount, obitwrapper<obwT>& boutput);

// reverse op
template <typename T, typename ibwT, size_t alignment = 16>
void bplaneDecode(T* data, size_t datasize, size_t pcount, ibitwrapper<ibwT>& binput);

template <typename srcT, typename dstT, size_t alignment = 16>
void accumulate_bplane(srcT* bplane, dstT* dst, size_t dst_length, std::function<void(dstT&, srcT)> accop);



// implementation section

template <typename T>
inline vlw_t combine_bword(T raw, T select_mask) {
	// UB when mask is all one bits 0xfffffff... due to shift equal to 
	// underlying type bitsize
	// 
	// TODO: implement contraints / nesessary casts
	// requires select_mask to be unsigned type

	// return a simple type that is represented by 2 full width
	// gprs, caller then casts/narrows the return value as needed

	size_t code{ 0 };
	size_t length{ 0 };
	size_t mask = -1;
	while (select_mask != 0) {
		size_t skip_count = std::countr_zero(select_mask);
		select_mask >>= skip_count;
		raw >>= skip_count;

		size_t select_count = std::countr_one(select_mask);
		code |= ((raw & (~(mask << select_count))) << length);

		length += select_count;
		raw >>= select_count;
		select_mask >>= select_count;
	}
	return { length, code };
}

template <typename T>
inline T decompose_bword(T packed, T unpack_mask) {
	T result = 0;
	size_t shift = 0;
	while (unpack_mask > 0) {
		size_t skip = std::countr_zero(unpack_mask);
		shift += skip;
		unpack_mask >>= skip;
		size_t count = std::countr_one(unpack_mask);
		result |= ((~(((T)(-1)) << count)) & packed) << shift;
		shift += count;
		unpack_mask >>= count;
		packed >>= count;
	}
	return result;
}

template <typename T, typename obwT, size_t alignment, bool omit_left_shifts>
void bplane(T* data, size_t size, size_t b, obitwrapper<obwT>& ostream, 
		std::function<void(T&)> transform) {
	// will have to perform integral promotion on the whole vector unit 
	// otherwise, meaning allocating additional vector registers. Or 
	// allocating several serial buffers of sizeof(T) as an array.
	static_assert(alignment <= (sizeof(T) << 3), 
		"Data loss: T is not capable of holding the number of bits equal to vector size.");
	std::array<std::make_unsigned_t<T>, alignment> masks;
	std::fill(masks.begin(), masks.end(), (((std::make_unsigned_t<T>)(0x01)) << b));

	std::array<T, alignment> rshifts;
	std::array<T, alignment> lshifts;
	std::array<std::make_unsigned_t<T>, alignment> buffer;
	for (ptrdiff_t i = 0; i < rshifts.size(); ++i) {
		std::make_signed_t<T> relative_shift = ((std::make_signed_t<decltype(b)>)(b)) - alignment + 1 + i;
		rshifts[i] = relu(relative_shift);
		if constexpr (!omit_left_shifts) {
			lshifts[i] = relu(-relative_shift);
		}
	}

	std::make_unsigned_t<sufficient_integral_i<(alignment >> 3)>> serial_buffer;

	for (ptrdiff_t i = 0; i < size; i += alignment) {
		if (i != 0) {
			ostream << vlw_t{ alignment, serial_buffer };
		}
		serial_buffer = 0;
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			buffer[j] = (data[i + j] & masks[j]);
			if constexpr (!omit_left_shifts) {
				buffer[j] <<= lshifts[j];
			}
			buffer[j] >>= rshifts[j];
			transform(data[i + j]);
		}
		for (ptrdiff_t j = 0; j < alignment; ++j) {
			serial_buffer |= buffer[j];
		}
	}

	constexpr size_t v_mask = alignment - 1;
	size_t v_size_tail = size & v_mask;
	v_size_tail += zeropred(alignment, v_size_tail == 0);
	
	serial_buffer >>= alignment - v_size_tail;
	ostream << vlw_t{ v_size_tail, serial_buffer };
}

template <size_t b, typename T, typename obwT, size_t alignment>
void bplane_static(T* data, size_t size, obitwrapper<obwT>& ostream, 
		std::function<void(std::type_identity_t<T>&)> transform) {
	if constexpr (b >= alignment) {
		bplane<T, obwT, alignment, true>(data, size, b, ostream, transform);
	} else {
		bplane<T, obwT, alignment, false>(data, size, b, ostream, transform);
	}
}

template <typename T, typename obwT, size_t alignment>
void bplanev4(T* data, size_t size, size_t belder, std::array<std::reference_wrapper<obitwrapper<obwT>>, 4> ostreams) {
	size_t b = relu(((std::make_signed_t<decltype(belder)>)(belder)) - ostreams.size() + 1);
	std::make_unsigned_t<T> mask = ((T)(0x01) << b);

	std::array<T, ostreams.size()> indicies{ 0 };
	size_t elder_shift = std::min(belder, ostreams.size() - 1);
	//  & (i < indicies.size()) is redandunt
	for (ptrdiff_t i = 0; (elder_shift > 0) & (i < indicies.size()); ++i) {
		indicies[i] = elder_shift;
		--elder_shift;
	}

	std::array<T, alignment> rshifts;
	std::array<T, alignment> lshifts;
	std::array<std::make_unsigned_t<T>, alignment> buffer;
	for (ptrdiff_t i = 0; i < rshifts.size(); ++i) {
		std::make_signed_t<T> relative_shift = ((std::make_signed_t<decltype(b)>)(b)) - alignment + 1 + i;
		rshifts[i] = relu(relative_shift);
		lshifts[i] = relu(-relative_shift);
	}

	std::array<std::make_unsigned_t<sufficient_integral_i<(alignment >> 3)>>, ostreams.size()> serial_buffers;

	for (ptrdiff_t i = 0; i < size; i += alignment) {
		for (ptrdiff_t k = 0; k < serial_buffers.size(); ++k) {
			serial_buffers[k] = 0;
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				buffer[j] = (data[i + j] & (mask << indicies[k]));
			}
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				// TODO: check if promotion needed to integral sufficient to hold shifted value
				// TODO: compile-time checks to prevent the following
				// possible data loss if alignment > (sizeof(T) << 3)
				serial_buffers[k] |= (buffer[j] << lshifts[j]) >> (rshifts[j] + indicies[k]);
			}
		}
		for (ptrdiff_t k = 0; k < serial_buffers.size(); ++k) {
			ostreams[k].get() << vlw_t{ alignment, serial_buffers[k] };
		}
	}
}

template <typename T, typename obwT, size_t alignment>
void bplaneEncode(T* data, size_t datasize, size_t pindex, size_t pcount, obitwrapper<obwT>& boutput) {
	// TODO: underlying obitwrapper type
	constexpr size_t bufferbsize = sizeof(obwT) << 3;
	// TODO: Probably intializing temporal buffers with optimal size_t underlying type is desirable, to make sure
	// that forwarding content from temporal buffers to the output bit stream is efficient. That requires
	// initializing array of deque<size_t> and then passing tuple with the first element of type corresponding
	// to the output bitstream, and the rest 3 temporal bit streams as an array of bitstreams with type size_t
	//
	std::array<std::deque<obwT>, 3> vobuffers;
	auto ocallback = [&] <ptrdiff_t i> (obwT item) -> void {
		vobuffers[i].push_back(item);
	};
	std::array<obitwrapper<obwT>, 3> vwrappers {
		obitwrapper<obwT>(std::bind(&decltype(ocallback)::template operator()<0>, &ocallback, std::placeholders::_1)),
		obitwrapper<obwT>(std::bind(&decltype(ocallback)::template operator()<1>, &ocallback, std::placeholders::_1)),
		obitwrapper<obwT>(std::bind(&decltype(ocallback)::template operator()<2>, &ocallback, std::placeholders::_1))
	};
	std::array<std::reference_wrapper<obitwrapper<obwT>>, 4> vboutputs = {
		std::ref(boutput),
		std::ref(vwrappers[0]),
		std::ref(vwrappers[1]),
		std::ref(vwrappers[2])
	};

	while (pcount > 0) {
		size_t pstep = std::min(pcount, (decltype(pcount))(4));
		if (pstep > 1) {
			bplanev4(data, datasize, pindex, vboutputs);
			for (ptrdiff_t i = 1; i < pstep; ++i) {
				ptrdiff_t buffer_index = i - 1;
				if (boutput.dirty()) {
					// executes arithmetic shifts inside obitwrapper::operator<< with shift size
					// equal bitsize of an underlying type, resulting in UB (riscv and x86 limit
					// shift amount to 5 bit for 32-bit and to 6 bit for 64-bit integrals) if 
					// boutput is clean and boutput's underlying type is at least the the same 
					// width as the current buffer's one
					while (!(vobuffers[buffer_index].empty())) {
						boutput << vlw_t{ bufferbsize, vobuffers[buffer_index].front() };
						vobuffers[buffer_index].pop_front();
					}
				} else {
					while (!(vobuffers[buffer_index].empty())) {
						boutput.flush_word(vobuffers[buffer_index].front());
						vobuffers[buffer_index].pop_front();
					}
				}

				size_t bufshift = vboutputs[i].get().ocount();
				// The branch below should be always hit because of unconditional flush. But it is handled 
				// properly because 0-length word is inserted into the stream if the buffer was empty before
				// flashing. Yet deque::pop_front() is UB when called on non-empty container, may make sense
				// to keep branch and mark it as [[likely]] and handle errors in else branch (but should never
				// happen).
				/// vboutputs[i].get().flush();
				/// boutput << vlw_t{ bufferbsize - bufshift, (vobuffers[buffer_index].front() >> bufshift) };
				/// vobuffers[buffer_index].pop_front();
				vboutputs[i].get().flush();
				[[likely]]
				if (!(vobuffers[buffer_index].empty())) {
					boutput << vlw_t{ bufferbsize - bufshift, (vobuffers[buffer_index].front() >> bufshift) };
					vobuffers[buffer_index].pop_front();
				} else {
					// should never happen
					// TODO: C++23 std::unreachable();
					throw "UB!";
				}
			}
		} else {
			bplane(data, datasize, pindex, boutput);
		}
		pindex -= pstep;
		pcount -= pstep;
	}
}

template <typename T, typename ibwT, size_t alignment>
void bplaneDecode(T* data, size_t datasize, size_t pcount, ibitwrapper<ibwT>& binput) {
	std::array<T, alignment> rshifts;
	for (ptrdiff_t i = 0; i < alignment; ++i) {
		rshifts[i] = alignment - i - 1;
	}

	constexpr size_t v_mask = alignment - 1;
	size_t v_size_truncated = datasize & (~v_mask);
	size_t v_size_tail = datasize & v_mask;
	ptrdiff_t v_offset_rshifts = alignment - v_size_tail;

	std::make_unsigned_t<sufficient_integral_i<(alignment >> 3)>> serial_buffer;
	for (ptrdiff_t b = 0; b < pcount; ++b) {
		ptrdiff_t i = 0;
		for (; i < v_size_truncated; i += alignment) {
			serial_buffer = binput.extract(alignment);
			std::array<decltype(serial_buffer), alignment> buffer;
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				buffer[j] = serial_buffer >> rshifts[j];
			}
			for (ptrdiff_t j = 0; j < alignment; ++j) {
				data[i + j] = (data[i + j] << 1) | (buffer[j] & 0x01);
			}
		}
		if (v_size_tail > 0) {
			serial_buffer = binput.extract(v_size_tail);
			for (ptrdiff_t j = 0; j < v_size_tail; ++j) {
				data[i + j] = (data[i + j] << 1) | ((serial_buffer >> rshifts[v_offset_rshifts + j]) & 0x01);
			}
		}
	}
}

template <typename srcT, typename dstT, size_t alignment>
void accumulate_bplane(srcT* bplane, dstT* dst, size_t dst_length, 
		std::function<void(dstT&, srcT)> accop/*, std::function<void(srcT&)> src_set_state*/) {
	constexpr size_t bplane_index_shift = std::bit_width(sizeof(srcT)) - 1 + 3;
	constexpr size_t serial_max_shift = (sizeof(srcT) << 3) - alignment;
	constexpr srcT serial_mask = ~((-1) << alignment);

	using vitem_t = std::make_unsigned_t<sufficient_integral_i<(alignment >> 3)>>;

	std::array<vitem_t, alignment> rshifts;
	for (ptrdiff_t i = 0; i < alignment; ++i) {
		rshifts[i] = alignment - i - 1;
	}

	vitem_t serial_buffer;
	constexpr size_t i_step = sizeof(srcT) << 3;
	for (ptrdiff_t i = 0; i < dst_length; i += i_step) {
		ptrdiff_t bplane_index = i >> bplane_index_shift;
		auto bplane_item = bplane[bplane_index];
		for (ptrdiff_t j = 0; j < i_step; j += alignment) {
			size_t serial_shift = serial_max_shift - j;
			serial_buffer = (bplane_item >> serial_shift) & serial_mask;
			std::array<decltype(serial_buffer), alignment> buffer;
			for (ptrdiff_t k = 0; k < alignment; ++k) {
				buffer[k] = serial_buffer >> rshifts[k];
				accop(dst[i + j + k], (buffer[k] & 0x01));
			}
		}
		// src_set_state(bplane[bplane_index]);
		bplane[bplane_index] = 0;
	}
}
