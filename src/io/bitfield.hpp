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

	using bitfield_buffer_t = std::array<uint8_t, size>;

private:
	std::array<uint8_t, size> bitfield_buffer = { 0 };
	std::array<bool, fields_num> processed = { false };

protected:
	~bitfield() = default;
	bitfield() = default;
	bitfield(const uint8_t* data) {
		std::copy_n(data, size, bitfield_buffer.begin());
	}

	consteval static size_t name_to_index(std::string_view target_name) {
		static_assert(bitfield::fields_num > 0, "No fields defined");
		return bitfield::name_to_index_impl<fields_num - 1>(target_name);
	}

	template <size_t index, typename T>
	void set_bitfield(T value) {
		this->set_bitfield_impl(bitfield::field_description<index>().alloc, value);
		this->processed[index] = true;
	}

	template <size_t index>
	auto get_bitfield() const {
		using rT = decltype(bitfield::field_description<index>().default_value);
		if constexpr (std::is_same_v<rT, bool>) {
			// because bool is integral type, but does not support integral 
			// operations like bitshifts and cannot be used as an argument for 
			// std::make_signed or std::make_unsigned
			return this->get_bitfield_impl<uint8_t>(bitfield::field_description<index>().alloc);
		} else {
			return this->get_bitfield_impl<rT>(bitfield::field_description<index>().alloc);
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

		size_t balign_shift = (balloc.offset - balloc.width) & bindex_mask;
		value_bv.compound = std::rotl(value_bv.compound, balign_shift);
		mask_bv.compound = std::rotl(mask_bv.compound, balign_shift);

		ptrdiff_t data_offset = (balloc.offset + relu(balloc.width - (sizeof(uT) << 3))) >> 3;

		if constexpr (std::endian::native != std::endian::big) {
			// little-endian implementaion considered as default
			uint16_t mask_msb = mask_bv.bytes[sizeof(uT) - 1];
			mask_msb <<= balign_shift;
			
			uint8_t mask_byte = mask_msb >> ((sizeof(uint16_t) - 1) << 3);
			uint8_t value_byte = value_bv.bytes[0];

			this->bitfield_buffer[data_offset] = (this->bitfield_buffer[data_offset] & (~mask_byte)) ^ (value_byte & mask_byte);
			++data_offset;

			for (ptrdiff_t i = sizeof(uT) - 1; i >= 0; --i) {
				mask_byte = mask_bv.bytes[i];
				value_byte = value_bv.bytes[i];

				this->bitfield_buffer[data_offset] = (this->bitfield_buffer[data_offset] & (~mask_byte)) ^ (value_byte & mask_byte);
				++data_offset;
			}
		} else {
			uint16_t mask_msb = mask_bv.bytes[0];
			mask_msb <<= balign_shift;

			uint8_t mask_byte = mask_msb >> ((sizeof(uint16_t) - 1) << 3);
			uint8_t value_byte = value_bv.bytes[sizeof(uT) - 1];

			this->bitfield_buffer[data_offset] = (this->bitfield_buffer[data_offset] & (~mask_byte)) ^ (value_byte & mask_byte);
			++data_offset;

			for (ptrdiff_t i = 0; i < sizeof(uT); ++i) {
				mask_byte = mask_bv.bytes[i];
				value_byte = value_bv.bytes[i];

				this->bitfield_buffer[data_offset] = (this->bitfield_buffer[data_offset] & (~mask_byte)) ^ (value_byte & mask_byte);
				++data_offset;
			}
		}

		// |.+.....|	offset
		// |...+...|	width
		//			 -= 2		=> mask rotl 2
		// 
		// 
		// |.....*.|	offset
		// |...+...|	width
		//			 -= 6		=> mask rotl 6
		// 
	}

	template <typename T>
	T get_bitfield_impl(bitfield_allocation balloc) const {
		using uT = std::make_unsigned_t<T>;		// but T is meant to be unsigned anyway...
		using sT = std::make_signed_t<T>;

		constexpr size_t bindex_mask = ~((-1) << 3);

		bytes_view<uT> mask_bv{ .compound = (uT)(~((sT)(-1) << balloc.width)) };
		bytes_view<uT> result_bv{ .compound = 0 };
		size_t ballign_shift = (balloc.offset + balloc.width) & bindex_mask;

		// ptrdiff_t data_offset = (balloc.offset + balloc.width) - ((sizeof(uT) + 1) << 3);
		// data_offset <<= 3;
		ptrdiff_t data_offset = (balloc.offset + relu(balloc.width - (sizeof(uT) << 3))) >> 3;

		uint8_t result_msb = this->bitfield_buffer[data_offset];
		uint8_t mask_msb = (uint8_t)((-1) << ballign_shift);
		++data_offset;

		if constexpr (std::endian::native != std::endian::big) {
			for (ptrdiff_t i = sizeof(uT) - 1; i >= 0; --i) {
				result_bv.bytes[i] = this->bitfield_buffer[data_offset];
				++data_offset;
			}
		} else {
			for (ptrdiff_t i = 0; i < sizeof(uT); ++i) {
				result_bv.bytes[i] = this->bitfield_buffer[data_offset];
				++data_offset;
			}
		}

		result_bv.bytes[0] = (result_bv.bytes[0] & mask_msb) ^ (result_msb & (~mask_msb));
		result_bv.compound = std::rotr(result_bv.compound, ballign_shift);
		result_bv.compound &= mask_bv.compound;

		return signext(result_bv.compound, balloc.width);
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
