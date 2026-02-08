#pragma once

#include <string_view>
#include <bit>
#include <algorithm>
#include <type_traits>
#include <stdexcept>
#include <cstdint>

#include "utils.hpp"

// bit-fields are underspecified in C++; the standard allows both msb-to-lsb
// and lsb-to-msb allocations of adjacent consequtive bit-fields within an
// allocation unit, and allows splitting every bit-field into separate 
// allocation unit. This makes bit-fields layout unpredictible on different 
// platforms, and therefore bit-fields appear to be useless for protocol 
// definitions.


using namespace std::literals;

struct bitfield_allocation {
	ptrdiff_t offset;
	size_t width;
};

template <typename T>
struct bitfield_description {
	std::string_view name;
	bitfield_allocation alloc;
	T default_value;
};

template<typename T>
struct bitfield_traits;

template <typename... Args>
using fields_description_t = std::tuple<bitfield_description<Args>...>;

template <typename Derived>
class bitfield {
protected:
	using defined_fields_t = decltype(bitfield_traits<Derived>::fields);

private:
	template <size_t index>
	constexpr static std::tuple_element_t<index, defined_fields_t> field_description() {
		return std::get<index>(bitfield_traits<Derived>::fields);
	}

	template <size_t index>
	consteval static size_t max_field_offset(size_t max_offset = 0) {
		bitfield_allocation curr_alloc = bitfield::field_description<index>().alloc;
		max_offset = std::max(max_offset, curr_alloc.offset + curr_alloc.width);
		if constexpr (index == 0) {
			return max_offset;
		} else {
			return bitfield::max_field_offset<index - 1>(max_offset);
		}
	}

	consteval static size_t buffer_size() {
		size_t max_offset_allocated = 0;
		if (bitfield::fields_num > 0) {
			max_offset_allocated = bitfield::max_field_offset<bitfield::fields_num - 1>();
		}

		size_t bindex_mask = ~((-1) << 3);
		return (max_offset_allocated + bindex_mask) >> 3;
	}

protected:
	constexpr static size_t fields_num = std::tuple_size_v<defined_fields_t>;
	constexpr static size_t size = bitfield::buffer_size();

	using bitfield_buffer_t = std::array<std::byte, size>;

private:
	std::array<std::byte, size> bitfield_buffer = { std::byte{0} };
	std::array<bool, fields_num> processed = { false };

protected:
	~bitfield() = default;
	bitfield() = default;
	bitfield(std::span<std::byte, size> data) {
		std::copy_n(data.begin(), size, bitfield_buffer.begin());
	}

	consteval static size_t name_to_index(std::string_view target_name) {
		static_assert(bitfield::fields_num > 0, "No fields defined");
		return bitfield::name_to_index_impl<fields_num - 1>(target_name);
	}

	template <size_t index, typename T>
	void set_bitfield(T value) {
		constexpr bitfield_description description = bitfield::field_description<index>();
		this->set_bitfield_impl(description.alloc, value);
		this->processed[index] = true;
	}

	template <size_t index>
	auto get_bitfield() const {
		constexpr bitfield_description description = bitfield::field_description<index>();
		using rT = decltype(description.default_value);
		if constexpr (std::is_same_v<rT, bool>) {
			// because bool is integral type, but does not support arithmetic 
			// operations like bitshifts and cannot be used as an argument for 
			// std::make_signed or std::make_unsigned
			return this->get_bitfield_impl<uint8_t>(description.alloc);
		} else {
			return this->get_bitfield_impl<rT>(description.alloc);
		}
	}

	template <size_t index>
	void set_default() {
		using rT = decltype(bitfield::field_description<index>().default_value);
		if constexpr (std::is_same_v<rT, bool>) {
			// see get_bitfield
			this->set_bitfield<index>((uint8_t)(bitfield::field_description<index>().default_value));
		} else {
			this->set_bitfield<index>(bitfield::field_description<index>().default_value);
		}
	}

	template <size_t index>
	auto get_default() const {
		using rT = decltype(bitfield::field_description<index>().default_value);
		return bitfield::field_description<index>().default_value;
	}

	const bitfield_buffer_t& commit() {
		if (std::any_of(this->processed.cbegin(), this->processed.cend(),
				[](bool param) -> bool { return param == false; })) {
			auto defaults_assigner = [this]<size_t index>(bool) -> void {
				// param is fictive, for unroll purposes only
				if (this->processed[index] == false) {
					this->set_default<index>();
				}
			};
 
			std::apply(
				[&](const auto& ... args) {
					unroll<false, left>::apply(defaults_assigner, args...);
				}, 
				this->processed);
		}

		return this->bitfield_buffer;
	}

private:
	template <size_t index>
	consteval static size_t name_to_index_impl(std::string_view target) {
		if (bitfield::field_description<index>().name == target) {
			return index;
		}

		if constexpr (index == 0) {
			throw std::out_of_range("Field name was not defined");
		} else {
			return bitfield::name_to_index_impl<index - 1>(target);
		}
	}

	template <typename T>
	void set_bitfield_impl(bitfield_allocation balloc, T value) {
		// bufferized implementation, performs excessive writes (dependent
		// on the size of T type). Requires both little-endian and big-endian
		// implementations as depends on byte order in T type. Assuming the 
		// function is inlined and balloc parameter is likely constexpr, many 
		// values may be known at compile time, and excessive writes may be 
		// optimized away by compiler. Loops can be effectuvely unrolled as 
		// constexpr arguments are used for loop condition.
		// 
		
		// TODO: unsigned constraint for T value
		using uT = std::make_unsigned_t<T>;		// but T is meant to be unsigned anyway...
		using sT = std::make_signed_t<T>;

		constexpr size_t bindex_mask = ~((-1) << 3);

		bytes_view<uT> value_bv{ .compound = value };
		bytes_view<uT> mask_bv{ .compound = (uT)(~((sT)(-1) << balloc.width)) };

		size_t balign_shift = (-(ptrdiff_t)(balloc.offset + balloc.width)) & bindex_mask;
		uint16_t mask_msb = mask_bv.compound >> ((sizeof(uT) - 1) << 3);
		mask_msb <<= balign_shift;
		mask_msb >>= (1 << 3);
		mask_bv.compound <<= balign_shift;
		value_bv.compound = std::rotl(value_bv.compound, balign_shift);
		// mask_bv.compound = std::rotl(mask_bv.compound, balign_shift);

		// ptrdiff_t data_offset = (balloc.offset + relu(balloc.width - (sizeof(uT) << 3))) >> 3;
		ptrdiff_t virtual_data_offset = ((ptrdiff_t)(balloc.offset + balloc.width) - (ptrdiff_t)(sizeof(uT) << 3) - 1) >> 3;
		// TODO: MAJOR:
		// result of sizeof expression has type size_t. rank of size_t is greater than ptrdiff_t, and 
		// likely greater than other wide signed integer types... That means that every time we 
		// subtract sizeof() value from some signed value, assuming that negative result is possible, 
		// we introduce UB due to unsigned underflow.
		// Seems we'll need to move to intptr_t/uintptr_t...
		// But still rank of size_t may be greater than any of them...
		// 

		if constexpr (std::endian::native != std::endian::big) {
			// little-endian implementaion considered as default
			// 
			// uint16_t mask_msb = std::to_integer<uint16_t>(mask_bv.bytes[sizeof(uT) - 1]);
			// mask_msb <<= balign_shift;
			// std::byte mask_byte { (uint8_t)(mask_msb >> ((sizeof(uint16_t) - 1) << 3)) };

			// uint8_t mask_msb = uint8_t(~((-1) << balign_shift)) & 
			// 	std::to_integer<uint8_t>(mask_bv.bytes[0]);
			// mask_bv.compound &= ~((sT)mask_msb);
			
			std::byte mask_byte { (uint8_t)(mask_msb) };
			std::byte value_byte { value_bv.bytes[0] };

			ptrdiff_t data_offset = virtual_data_offset &
				(~(virtual_data_offset >> ((sizeof(virtual_data_offset) << 3) - 1)));
			this->bitfield_buffer[data_offset] = (this->bitfield_buffer[data_offset] & (~mask_byte)) ^ (value_byte & mask_byte);
			// data_offset += mask_msb > 0;
			++virtual_data_offset;

			// ++data_offset;
			// data_offset &= -(data_offset == size);	// boundary wrap

			for (ptrdiff_t i = sizeof(uT) - 1; i >= 0; --i) {
				mask_byte = mask_bv.bytes[i];
				value_byte = value_bv.bytes[i];

				data_offset = virtual_data_offset & 
					(~(virtual_data_offset >> ((sizeof(virtual_data_offset) << 3) - 1)));
				this->bitfield_buffer[data_offset] = (this->bitfield_buffer[data_offset] & (~mask_byte)) ^ (value_byte & mask_byte);
				// ++data_offset;
				// data_offset += std::to_integer<uint8_t>(mask_byte) > 0;
				++virtual_data_offset;
			}
		} else {
			ptrdiff_t data_offset = virtual_data_offset &
				(virtual_data_offset >> ((sizeof(virtual_data_offset) << 3) - 1));

			// uint16_t 
			mask_msb = std::to_integer<uint16_t>(mask_bv.bytes[0]);
			mask_msb <<= balign_shift;

			std::byte mask_byte { (uint8_t)(mask_msb >> ((sizeof(uint16_t) - 1) << 3)) };
			std::byte value_byte { value_bv.bytes[sizeof(uT) - 1] };

			this->bitfield_buffer[data_offset] = (this->bitfield_buffer[data_offset] & (~mask_byte)) ^ (value_byte & mask_byte);
			++data_offset;

			for (ptrdiff_t i = 0; i < sizeof(uT); ++i) {
				mask_byte = mask_bv.bytes[i];
				value_byte = value_bv.bytes[i];

				this->bitfield_buffer[data_offset] = (this->bitfield_buffer[data_offset] & (~mask_byte)) ^ (value_byte & mask_byte);
				++data_offset;
			}
		}

		// (-offset - width) modulo-like 8 [effectively take 3 least significant bits]
		// (-(offset + width)) modulo-like 8
		// 
		// 0	   8
		// |.+.....|	offset		(-)2
		// |...+...|	width		4
		//			 -= 2		=> mask rotl 2
		// 
		// 
		// |.....*.|	offset		(-)6
		// |...+...|	width		4
		//			 -= 6		=> mask rotl 6
		// 
		// but buffer and bit indexing is msb first, bits are numbered left-to-right; 
		// offset should be expressed as compliment to multiple of 8 (bits in byte).
		// (0 - offset) works well.
		// 

		//			|.......|.......|.......|	[24 bits]
		//			|_______|_______|___+...|	[offset 0, width 20]
		//	|.......|....+__|_______|_______|	[32 bits, 20 bits significant for value]
		// 
		//	|.......|_______|_______|___+...|
		//	 ???????|_______|_______|___+...|
		// 
		// 
		//											|.......|.......|.......|	[24 bits]
		//											|!......|.......|.......|	[offset 0, width 1]
		//	|.......|.......|.......|.......|.......|.......|.......|.......!	[64 bits, 1 bit significant]
		// 
		//	|.......|.......|.......|.......|.......|!......|.......|.......|
		//	 ???????????????????????????????????????|!......|.......|.......|
		// 
		// 
		// 
		//					|.......|.......|	[16 bits]
		//					|!......|.......|	[offset 0, width 1]
		//	|.......|.......|.......|.......!	[32 bits, 1 bit significant for value]
		// 
		//	|.......|.......|!......|.......|
		//	 ???????????????|!......|.......|
		// 
	}

	template <typename T>
	T get_bitfield_impl(bitfield_allocation balloc) const {
		using uT = std::make_unsigned_t<T>;		// but T is meant to be unsigned anyway...
		using sT = std::make_signed_t<T>;

		constexpr size_t bindex_mask = ~((-1) << 3);

		bytes_view<uT> mask_bv{ .compound = (uT)(~((sT)(-1) << balloc.width)) };
		bytes_view<uT> result_bv{ .compound = 0 };
		size_t ballign_shift = (-(ptrdiff_t)(balloc.offset + balloc.width)) & bindex_mask;

		// ptrdiff_t data_offset = (balloc.offset + balloc.width) - ((sizeof(uT) + 1) << 3);
		// data_offset <<= 3;
		// ptrdiff_t data_offset = (balloc.offset + relu(balloc.width - (sizeof(uT) << 3))) >> 3;
		ptrdiff_t virtual_data_offset = ((ptrdiff_t)(balloc.offset + balloc.width) - (ptrdiff_t)(sizeof(uT) << 3) - 1) >> 3;

		ptrdiff_t data_offset = virtual_data_offset &
			(~(virtual_data_offset >> ((sizeof(virtual_data_offset) << 3) - 1)));
		std::byte result_msb { this->bitfield_buffer[data_offset] };
		std::byte mask_msb { (uint8_t)~((-1) << ballign_shift) };
		// ++data_offset;
		++virtual_data_offset;

		if constexpr (std::endian::native != std::endian::big) {
			for (ptrdiff_t i = sizeof(uT) - 1; i >= 0; --i) {
				data_offset = virtual_data_offset &
					(~(virtual_data_offset >> ((sizeof(virtual_data_offset) << 3) - 1)));
				result_bv.bytes[i] = this->bitfield_buffer[data_offset];
				// ++data_offset;
				++virtual_data_offset;
			}
		} else {
			ptrdiff_t data_offset = virtual_data_offset &
				(virtual_data_offset >> ((sizeof(virtual_data_offset) << 3) - 1));

			for (ptrdiff_t i = 0; i < sizeof(uT); ++i) {
				result_bv.bytes[i] = this->bitfield_buffer[data_offset];
				++data_offset;
			}
		}

		result_bv.bytes[0] = (result_bv.bytes[0] & (~mask_msb)) ^ (result_msb & mask_msb);
		result_bv.compound = std::rotr(result_bv.compound, ballign_shift);
		result_bv.compound &= mask_bv.compound;

		// as for now, there's no 2-complement or signed fields in the protocol.
		// if constexpr (std::is_signed_v<T>) {
		//	result_bv.compound = signext(result_bv.compound, balloc.width);
		// }
		return result_bv.compound;
	}

	consteval static bool validate_fields() {
		bool result = true;
		if constexpr (fields_num > 0) {
			constexpr size_t start_index = fields_num - 1;
			result &= validate_field_offsets<start_index>();
			result &= validate_fields_order<start_index>();
			result &= validate_defaults_size<start_index>();
		}
		return result;
	}

	template <size_t index>
	consteval static bool validate_field_offsets() {
		if (bitfield::field_description<index>().alloc.offset < 0) {
			return false;
		}

		if constexpr (index == 0) {
			return true;
		} else {
			return bitfield::validate_field_offsets<index - 1>();
		}
	}

	template <size_t index>
	consteval static bool validate_fields_order(bitfield_allocation next_alloc = bitfield::field_description<index>().alloc) {
		bitfield_allocation curr_alloc = bitfield::field_description<index>().alloc;
		if (curr_alloc.offset > next_alloc.offset) {
			return false;
		}

		if constexpr (index == 0) {
			return true;
		} else {
			return bitfield::validate_fields_order<index - 1>(curr_alloc);
		}
	}

	template <size_t index>
	consteval static bool validate_defaults_size() {
		size_t bsize_default = sizeof(bitfield::field_description<index>().default_value) << 3;
		size_t field_width = bitfield::field_description<index>().alloc.width;
		if (field_width > bsize_default) {
			return false;
		}

		if constexpr (index == 0) {
			return true;
		} else {
			return bitfield::validate_defaults_size<index - 1>();
		}
	}

	static_assert(validate_fields() == true);
};
