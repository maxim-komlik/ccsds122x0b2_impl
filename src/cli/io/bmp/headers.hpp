#pragma once

#include <string>
#include <array>
#include <string_view>
#include <span>
#include <algorithm>
#include <bit>
#include <functional>
#include <type_traits>
#include <cstdint>
#include <cstddef>

#include "common/utility.hpp"
#include "io/bitfield.hpp"

#include "exception/bmp/invalid_header.hpp"


// all fields (except text markers?) in the definitions below are little-endian. 
// Someone at Microsoft in the late 80s decided it's a good choice for "Device Independent Bitmap"


class bitmap_file_header;

template<>
struct bitfield_traits<bitmap_file_header> {
	static constexpr fields_description_t fields{
		bitfield_description{"Signature"sv,		{0, 16}, (uint16_t)(0x424b)},	// 'BM'
		bitfield_description{"Size"sv,			{16, 32}, (uint32_t)(0)},		// 
		bitfield_description{"reserved_001"sv,	{48, 16}, (uint16_t)(0)},		// 16 bit reserved
		bitfield_description{"reserved_002"sv,	{64, 16}, (uint16_t)(0)},		// 16 bit reserved
		bitfield_description{"ImageOffset"sv,	{80, 32}, (uint32_t)(0)},		// 
	};	// 14 bytes
};

class bitmap_file_header: private bitfield<bitmap_file_header> {
	static_assert(bitfield<bitmap_file_header>::size == 14);

private:
	template <size_t field_index>
	auto get_unsigned() const {
		return byteswap(this->get_bitfield<field_index>());
	}

	template <size_t field_index>
	void set_unsigned(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

public:
	bitmap_file_header() = default;
	bitmap_file_header(std::span<const std::byte, bitfield::size> raw_data) : bitfield(raw_data) {
		constexpr std::array known_signatures = std::invoke([]() constexpr {
				constexpr std::string_view values[] = {
					"BM"sv, 
					// and not sure if treat OS/2 variants as valid input...
					// "BA"sv, 
				};
				return std::to_array(values);
			});

		bool valid = true;
		valid &= (this->get_unsigned<name_to_index("reserved_001"sv)>() == 0);
		valid &= (this->get_unsigned<name_to_index("reserved_002"sv)>() == 0);
		valid &= (this->get_ImageOffset() <= this->get_Size());
		valid &= (known_signatures.cend() !=
			std::find(known_signatures.cbegin(), known_signatures.cend(), this->get_Signature()));

		if (!valid) {
			// TODO: throw, not a valid bmp
			throw cli::bmp::header_invalid{};
		}
	}

	std::string get_Signature() const {
		uint16_t signature_raw = this->get_bitfield<name_to_index("Signature"sv)>();
		if constexpr (std::endian::native != std::endian::big) {
			signature_raw = byteswap(signature_raw);
		}

		char* signature_begin = reinterpret_cast<char*>(&signature_raw);
		// char* signature_end = signature_begin + sizeof(signature_raw);

		// small string optimization, hopefully
		return std::string(signature_begin, sizeof(signature_raw));
	}

	size_t get_Size() const {
		// or return uintmax_t?

		return this->get_unsigned<name_to_index("Size"sv)>();
	}

	ptrdiff_t get_ImageOffset() const {
		return this->get_unsigned<name_to_index("ImageOffset"sv)>();
	}

	void set_Size(size_t value) {
		// or return uintmax_t?
		return this->set_unsigned<name_to_index("Size"sv)>(value);
		// constexpr size_t field_index = name_to_index("Size"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	void set_ImageOffset(ptrdiff_t value) {
		return this->set_unsigned<name_to_index("ImageOffset"sv)>(value);
		// constexpr size_t field_index = name_to_index("ImageOffset"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	const std::array<std::byte, bitfield::size>& commit() {
		// OS/2 bitmaps are not supported
		this->set_default<name_to_index("Signature"sv)>();

		this->set_default<name_to_index("reserved_001"sv)>();
		this->set_default<name_to_index("reserved_002"sv)>();

		bool valid = true;
		valid &= (this->get_ImageOffset() <= this->get_Size());

		if (!valid) {
			// TODO: throw, invalid header 
		}

		return bitfield::commit();
	}

	static constexpr size_t size() {
		return bitfield::size;
	}
};


enum class dib_header_type : uint32_t {
	// maps header type to it's size, that's the way headers are distinguished in BMP protocol
	bitmap_core_v1 = 12, 
	bitmap_core_v2 = 16, 
	// below can be combined with bitmap_core_v2 only, that is, every option substitutes previous ones and core_v2
	bitmap_info_v1 = 40, 
	bitmap_info_v2 = 52, 
	bitmap_info_v3 = 56, 
	bitmap_info_v4 = 108, 
	bitmap_info_v5 = 124
};


class bitmap_core_v1_header;
// used for single channel images

template<>
struct bitfield_traits<bitmap_core_v1_header> {
	static constexpr fields_description_t fields{
		bitfield_description{"HeaderSize"sv,	{0, 32}, (uint32_t)(0x12)},		// 
		bitfield_description{"ImageWidth"sv,	{32, 16}, (uint16_t)(0)},		// Image width
		bitfield_description{"ImageHeight"sv,	{48, 16}, (uint16_t)(0)},		// Image height
			// two fields above meant to be signed, but header fields are defined to be little-endian, 
			// therefore most significant bits containing sign extension are located at the end of the 
			// fields, and there's no straitforward way to parse it simply enough on big-endian 
			// architectures. So the definitions above are mapped to unsigned integers, and necessary 
			// conversions are done in the access interface implementation
		bitfield_description{"PlaneCount"sv,	{64, 16}, (uint16_t)(0)},		// 
		bitfield_description{"BitsPerPixel"sv,	{80, 16}, (uint32_t)(0)},		// 
	};	// 12 bytes
};


class bitmap_core_v1_header : private bitfield<bitmap_core_v1_header> {
	static_assert(bitfield::size == 12);

private:
	template <size_t field_index>
	auto get_unsigned() const {
		return byteswap(this->get_bitfield<field_index>());
	}

	template <size_t field_index>
	void set_unsigned(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	template <size_t field_index>
	auto get_signed() const {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		using s_value_t = std::make_signed_t<value_t>;

		return static_cast<s_value_t>(byteswap(this->get_bitfield<field_index>()));
	}

	template <size_t field_index>
	void set_signed(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		using s_value_t = std::make_signed_t<value_t>;

		return this->set_bitfield<field_index>(byteswap(static_cast<value_t>((s_value_t)(value))));
	}

public:
	bitmap_core_v1_header() = default;
	bitmap_core_v1_header(std::span<const std::byte, bitfield::size> raw_data) : bitfield(raw_data) {
		constexpr size_t max_bits_per_pixel = 64; // strictly speaking it's too large to be conformant, but should work well
		constexpr size_t byte_mask = (1 << 3) - 1;

		bool valid = true;
		valid &= (this->get_HeaderSize() == to_underlying(dib_header_type::bitmap_core_v1));
		// valid &= ((this->get_BitsPerPixel() & byte_mask) == 0);	// TODO: enforce or not?
		valid &= (this->get_BitsPerPixel() <= max_bits_per_pixel);

		if (!valid) {
			// TODO: throw, invalid header
			throw cli::bmp::header_invalid{};
		}
	}

	size_t get_HeaderSize() const {
		return this->get_unsigned<name_to_index("HeaderSize"sv)>();
	}

	ptrdiff_t get_ImageWidth() const {
		return this->get_signed<name_to_index("ImageWidth"sv)>();

		// constexpr size_t field_index = name_to_index("ImageWidth"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// using s_value_t = std::make_signed_t<value_t>;
		// 
		// return static_cast<s_value_t>(byteswap(this->get_bitfield<field_index>()));
	}

	ptrdiff_t get_ImageHeight() const {
		return this->get_signed<name_to_index("ImageHeight"sv)>();

		// constexpr size_t field_index = name_to_index("ImageHeight"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// using s_value_t = std::make_signed_t<value_t>;
		// 
		// return static_cast<s_value_t>(byteswap(this->get_bitfield<field_index>()));
	}

	size_t get_PlaneCount() const {
		return this->get_unsigned<name_to_index("PlaneCount"sv)>();
	}

	size_t get_BitsPerPixel() const {
		return this->get_unsigned<name_to_index("BitsPerPixel"sv)>();
	}

	void set_HeaderSize(size_t value) {
		return this->set_unsigned<name_to_index("HeaderSize"sv)>(value);
		// constexpr size_t field_index = name_to_index("HeaderSize"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	void set_ImageWidth(ptrdiff_t value) {
		return this->set_signed<name_to_index("ImageWidth"sv)>(value);
		// constexpr size_t field_index = name_to_index("ImageWidth"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// using s_value_t = std::make_signed_t<value_t>;
		// 
		// return this->set_bitfield<field_index>(byteswap(static_cast<value_t>((s_value_t)(value))));
	}

	void set_ImageHeight(ptrdiff_t value) {
		return this->set_signed<name_to_index("ImageHeight"sv)>(value);
		// constexpr size_t field_index = name_to_index("ImageHeight"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// using s_value_t = std::make_signed_t<value_t>;
		// 
		// return this->set_bitfield<field_index>(byteswap(static_cast<value_t>((s_value_t)(value))));
	}

	void set_PlaneCount(size_t value) {
		return this->set_unsigned<name_to_index("PlaneCount"sv)>(value);
		// constexpr size_t field_index = name_to_index("PlaneCount"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	void set_BitsPerPixel(size_t value) {
		return this->set_unsigned<name_to_index("BitsPerPixel"sv)>(value);
		// constexpr size_t field_index = name_to_index("BitsPerPixel"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	const std::array<std::byte, bitfield::size>& commit() {
		return bitfield::commit();
	}

	static constexpr size_t size() {
		return bitfield::size;
	}
};


class bitmap_core_v2_header;
// used for multi-channel images (3 or 4 channels), same as v1 but has 32-bit fields for image dimensions

template<>
struct bitfield_traits<bitmap_core_v2_header> {
	static constexpr fields_description_t fields{
		bitfield_description{"HeaderSize"sv,	{0, 32}, (uint32_t)(0x38)},		// 
		bitfield_description{"ImageWidth"sv,	{32, 32}, (uint32_t)(0)},		// Image width
		bitfield_description{"ImageHeight"sv,	{64, 32}, (uint32_t)(0)},		// Image height
			// two fields above meant to be signed, but header fields are defined to be little-endian, 
			// therefore most significant bits containing sign extension are located at the end of the 
			// fields, and there's no straitforward way to parse it simply enough on big-endian 
			// architectures. So the definitions above are mapped to unsigned integers, and necessary 
			// conversions are done in the access interface implementation
		bitfield_description{"PlaneCount"sv,	{96, 16}, (uint16_t)(0)},		// 
		bitfield_description{"BitsPerPixel"sv,	{112, 16}, (uint16_t)(0)},		// 
	};	// 16 bytes
};

class bitmap_core_v2_header : private bitfield<bitmap_core_v2_header> {
	static_assert(bitfield::size == 16);

private:
	template <size_t field_index>
	auto get_unsigned() const {
		return byteswap(this->get_bitfield<field_index>());
	}

	template <size_t field_index>
	void set_unsigned(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	template <size_t field_index>
	auto get_signed() const {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		using s_value_t = std::make_signed_t<value_t>;

		return static_cast<s_value_t>(byteswap(this->get_bitfield<field_index>()));
	}

	template <size_t field_index>
	void set_signed(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		using s_value_t = std::make_signed_t<value_t>;

		return this->set_bitfield<field_index>(byteswap(static_cast<value_t>((s_value_t)(value))));
	}

public:
	bitmap_core_v2_header() = default;
	bitmap_core_v2_header(std::span<const std::byte, bitfield::size> raw_data) : bitfield(raw_data) {
		constexpr size_t max_bits_per_pixel = 32; // limited by channel masks width
		constexpr size_t byte_mask = (1 << 3) - 1;

		constexpr std::array known_header_types = std::invoke([]() constexpr {
				dib_header_type values[]{
					// bitmap_core_v2 appears only as part of bitmap_info_v1 or later version, 
					// i.e., no raw bitmap_core_v2 header can be used as image header
					dib_header_type::bitmap_info_v1,
					dib_header_type::bitmap_info_v2,
					dib_header_type::bitmap_info_v3,
					dib_header_type::bitmap_info_v4,
					dib_header_type::bitmap_info_v5
				};
				return std::to_array(values);
			});

		bool valid = true;
		valid &= (known_header_types.cend() !=
			std::find(known_header_types.cbegin(), known_header_types.cend(), 
				static_cast<dib_header_type>(this->get_HeaderSize())));
		// valid &= ((this->get_BitsPerPixel() & byte_mask) == 0);	// TODO: enforce or not?
		valid &= (this->get_BitsPerPixel() <= max_bits_per_pixel);

		if (!valid) {
			// TODO: throw, invalid header
			throw cli::bmp::header_invalid{};
		}
	}

	size_t get_HeaderSize() const {
		return this->get_unsigned<name_to_index("HeaderSize"sv)>();
	}

	ptrdiff_t get_ImageWidth() const {
		return this->get_signed<name_to_index("ImageWidth"sv)>();
		// constexpr size_t field_index = name_to_index("ImageWidth"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// using s_value_t = std::make_signed_t<value_t>;
		// 
		// return static_cast<s_value_t>(byteswap(this->get_bitfield<field_index>()));
	}

	ptrdiff_t get_ImageHeight() const {
		return this->get_signed<name_to_index("ImageHeight"sv)>();
		// constexpr size_t field_index = name_to_index("ImageHeight"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// using s_value_t = std::make_signed_t<value_t>;
		// 
		// return static_cast<s_value_t>(byteswap(this->get_bitfield<field_index>()));
	}

	size_t get_PlaneCount() const {
		return this->get_unsigned<name_to_index("PlaneCount"sv)>();
	}

	size_t get_BitsPerPixel() const {
		return this->get_unsigned<name_to_index("BitsPerPixel"sv)>();
	}

	void set_HeaderSize(size_t value) {
		return this->set_unsigned<name_to_index("HeaderSize"sv)>(value);
		// constexpr size_t field_index = name_to_index("HeaderSize"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	void set_ImageWidth(ptrdiff_t value) {
		return this->set_signed<name_to_index("ImageWidth"sv)>(value);
		// constexpr size_t field_index = name_to_index("ImageWidth"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// using s_value_t = std::make_signed_t<value_t>;
		// 
		// return this->set_bitfield<field_index>(byteswap(static_cast<value_t>((s_value_t)(value))));
	}

	void set_ImageHeight(ptrdiff_t value) {
		return this->set_signed<name_to_index("ImageHeight"sv)>(value);
		// constexpr size_t field_index = name_to_index("ImageHeight"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// using s_value_t = std::make_signed_t<value_t>;
		// 
		// return this->set_bitfield<field_index>(byteswap(static_cast<value_t>((s_value_t)(value))));
	}

	void set_PlaneCount(size_t value) {
		return this->set_unsigned<name_to_index("PlaneCount"sv)>(value);
		// constexpr size_t field_index = name_to_index("PlaneCount"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	void set_BitsPerPixel(size_t value) {
		return this->set_unsigned<name_to_index("BitsPerPixel"sv)>(value);
		// constexpr size_t field_index = name_to_index("BitsPerPixel"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	const std::array<std::byte, bitfield::size>& commit() {
		return bitfield::commit();
	}

	static constexpr size_t size() {
		return bitfield::size;
	}
};


class bitmap_info_v1_header;
// defines multichannel content description via Compression field, but doesn't clarify channel
// bit allocation inside a pixel; therefore it's reasonable to choose not to support images with 
// info_v1 header only (at least info_v2 required)

template<>
struct bitfield_traits<bitmap_info_v1_header> {
	static constexpr fields_description_t fields{
		bitfield_description{"Compression"sv,			{0, 32}, (uint32_t)(0x0)},		// 
		bitfield_description{"ImageSize"sv,				{32, 32}, (uint32_t)(0)},		// 
		bitfield_description{"XPixelsPerMeter"sv,		{64, 32}, (uint32_t)(0)},		// 
		bitfield_description{"YPixelsPerMeter"sv,		{96, 32}, (uint32_t)(0)},		// 
		bitfield_description{"ColorTableItemCount"sv,	{128, 32}, (uint32_t)(0)},		// 
		bitfield_description{"ImportantColorCount"sv,	{160, 32}, (uint32_t)(0)},		// 
	};	// 24 bytes
};

class bitmap_info_v1_header : private bitfield<bitmap_info_v1_header> {
	static_assert(bitfield::size == 24);

public:

	enum class compression_type : uint32_t {
		rgb = 0x0, 
		rle8 = 0x01, 
		rle4 = 0x02, 
		bitfields = 0x03, 
		jpeg = 0x04, 
		png = 0x05, 
		alpha_bitfields = 0x06, 
		cmyk = 0x0b, 
		cmyk_rle8 = 0x0c, 
		cmyk_rle4 = 0x0d
	};

private:
	template <size_t field_index>
	auto get_unsigned() const {
		return byteswap(this->get_bitfield<field_index>());
	}

	template <size_t field_index>
	void set_unsigned(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

public:
	bitmap_info_v1_header() = default;
	bitmap_info_v1_header(std::span<const std::byte, bitfield::size> raw_data) : bitfield(raw_data) { }

	compression_type get_Compression() const {
		return static_cast<compression_type>(this->get_unsigned<name_to_index("Compression"sv)>());
	}

	size_t get_ImageSize() const {
		// or return uintmax_t?
		return this->get_unsigned<name_to_index("ImageSize"sv)>();
	}

	size_t get_XPixelsPerMeter() const {
		return this->get_unsigned<name_to_index("XPixelsPerMeter"sv)>();
	}

	size_t get_YPixelsPerMeter() const {
		return this->get_unsigned<name_to_index("YPixelsPerMeter"sv)>();
	}

	size_t get_ColorTableItemCount() const {
		return this->get_unsigned<name_to_index("ColorTableItemCount"sv)>();
	}

	size_t get_ImportantColorCount() const {
		return this->get_unsigned<name_to_index("ImportantColorCount"sv)>();
	}

	void set_Compression(compression_type value) {
		return this->set_unsigned<name_to_index("Compression"sv)>(to_underlying(value));
		// constexpr size_t field_index = name_to_index("Compression"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(to_underlying(value))));
	}

	void set_ImageSize(size_t value) {
		return this->set_unsigned<name_to_index("ImageSize"sv)>(value);
		// constexpr size_t field_index = name_to_index("ImageSize"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	void set_XPixelsPerMeter(size_t value) {
		return this->set_unsigned<name_to_index("XPixelsPerMeter"sv)>(value);
		// constexpr size_t field_index = name_to_index("XPixelsPerMeter"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	void set_YPixelsPerMeter(size_t value) {
		return this->set_unsigned<name_to_index("YPixelsPerMeter"sv)>(value);
		// constexpr size_t field_index = name_to_index("YPixelsPerMeter"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	void set_ColorTableItemCount(size_t value) {
		return this->set_unsigned<name_to_index("ColorTableItemCount"sv)>(value);
		// constexpr size_t field_index = name_to_index("ColorTableItemCount"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	void set_ImportantColorCount(size_t value) {
		return this->set_unsigned<name_to_index("ImportantColorCount"sv)>(value);
		// constexpr size_t field_index = name_to_index("ImportantColorCount"sv);
		// using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		// return this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

	const std::array<std::byte, bitfield::size>& commit() {
		return bitfield::commit();
	}

	static constexpr size_t size() {
		return bitfield::size;
	}
};


class bitmap_info_v2_header;

template<>
struct bitfield_traits<bitmap_info_v2_header> {
	static constexpr fields_description_t fields{
		bitfield_description{"MaskChannel1"sv,		{0, 32}, (uint32_t)(0x0)}, 	// R channel pixel mask
		bitfield_description{"MaskChannel2"sv,		{32, 32}, (uint32_t)(0)}, 	// G channel pixel mask
		bitfield_description{"MaskChannel3"sv,		{64, 32}, (uint32_t)(0)}, 	// B channel pixel mask
	};	// 12 bytes
};

class bitmap_info_v2_header : private bitfield<bitmap_info_v2_header> {
	static_assert(bitfield::size == 12);

private:
	template <size_t field_index>
	auto get_unsigned() const {
		return byteswap(this->get_bitfield<field_index>());
	}

	template <size_t field_index>
	void set_unsigned(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

public:
	bitmap_info_v2_header() = default;
	bitmap_info_v2_header(std::span<const std::byte, bitfield::size> raw_data) : bitfield(raw_data) { }

	uint32_t get_MaskChannel1() const {
		// return byteswap(this->get_bitfield<name_to_index("MaskChannel1"sv)>());
		return this->get_unsigned<name_to_index("MaskChannel1"sv)>();
	}

	uint32_t get_MaskChannel2() const {
		// return byteswap(this->get_bitfield<name_to_index("MaskChannel2"sv)>());
		return this->get_unsigned<name_to_index("MaskChannel2"sv)>();
	}

	uint32_t get_MaskChannel3() const {
		// return byteswap(this->get_bitfield<name_to_index("MaskChannel3"sv)>());
		return this->get_unsigned<name_to_index("MaskChannel3"sv)>();
	}

	void set_MaskChannel1(uint32_t value) {
		// return this->set_bitfield<name_to_index("MaskChannel1"sv)>(value);
		return this->set_unsigned<name_to_index("MaskChannel1"sv)>(value);
	}

	void set_MaskChannel2(uint32_t value) {
		// return this->set_bitfield<name_to_index("MaskChannel2"sv)>(value);
		return this->set_unsigned<name_to_index("MaskChannel2"sv)>(value);
	}

	void set_MaskChannel3(uint32_t value) {
		// return this->set_bitfield<name_to_index("MaskChannel3"sv)>(value);
		return this->set_unsigned<name_to_index("MaskChannel3"sv)>(value);
	}

	const std::array<std::byte, bitfield::size>& commit() {
		return bitfield::commit();
	}

	static constexpr size_t size() {
		return bitfield::size;
	}
};


class bitmap_info_v3_header;

template<>
struct bitfield_traits<bitmap_info_v3_header> {
	static constexpr fields_description_t fields{
		bitfield_description{"MaskChannel4"sv,		{0, 32}, (uint32_t)(0x0)}, 	// alpha channel pixel mask
	};	// 4 bytes
};
// channel masks as defined above are not necessary bound to RGBa, CMYK is valid as well

class bitmap_info_v3_header : private bitfield<bitmap_info_v3_header> {
	static_assert(bitfield::size == 4);

private:
	template <size_t field_index>
	auto get_unsigned() const {
		return byteswap(this->get_bitfield<field_index>());
	}

	template <size_t field_index>
	void set_unsigned(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

public:
	bitmap_info_v3_header() = default;
	bitmap_info_v3_header(std::span<const std::byte, bitfield::size> raw_data) : bitfield(raw_data) { }

	uint32_t get_MaskChannel4() const {
		return this->get_unsigned<name_to_index("MaskChannel4"sv)>();
	}

	void set_MaskChannel4(uint32_t value) {
		return this->set_unsigned<name_to_index("MaskChannel4"sv)>(value);
	}

	const std::array<std::byte, bitfield::size>& commit() {
		return bitfield::commit();
	}

	static constexpr size_t size() {
		return bitfield::size;
	}
};


class bitmap_info_v4_header;

template<>
struct bitfield_traits<bitmap_info_v4_header> {
	static constexpr fields_description_t fields{
		bitfield_description{"ColorSpaceType"sv,			{0, 32}, (uint32_t)(0x42475273)}, 	// 'RGBs'
		bitfield_description{"ColorSpaceEndPoint_001"sv,	{32, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"ColorSpaceEndPoint_002"sv,	{64, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"ColorSpaceEndPoint_003"sv,	{96, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"ColorSpaceEndPoint_004"sv,	{128, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"ColorSpaceEndPoint_005"sv,	{160, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"ColorSpaceEndPoint_006"sv,	{192, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"ColorSpaceEndPoint_007"sv,	{224, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"ColorSpaceEndPoint_008"sv,	{256, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"ColorSpaceEndPoint_009"sv,	{288, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"GammaChannel1"sv,				{320, 32}, (uint32_t)(0)}, 	// R gamma
		bitfield_description{"GammaChannel2"sv,				{352, 32}, (uint32_t)(0)}, 	// G gamma
		bitfield_description{"GammaChannel3"sv,				{384, 32}, (uint32_t)(0)}, 	// B gamma
		// gamma for channel 4 is not defined (alpha in RGBa or black in CMYK)
	};	// 52 bytes
};

class bitmap_info_v4_header : private bitfield<bitmap_info_v4_header> {
	static_assert(bitfield::size == 52);

public:
	enum class color_space : uint32_t {
		calibrated_rgb = 0x0, 
		sRGB = 0x73524742, 
		windows = 0x57696E20, 
		unknown = 0xffffffff
	};

private:
	template <size_t field_index>
	auto get_unsigned() const {
		return byteswap(this->get_bitfield<field_index>());
	}

	template <size_t field_index>
	void set_unsigned(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

public:
	bitmap_info_v4_header() = default;
	bitmap_info_v4_header(std::span<const std::byte, bitfield::size> raw_data) : bitfield(raw_data) { 
		// TODO: constraints on color space type?
	}

	std::string get_ColorSpaceType_string() const {
		uint32_t color_space_raw = this->get_bitfield<name_to_index("ColorSpaceType"sv)>();
		if constexpr (std::endian::native != std::endian::big) {
			color_space_raw = byteswap(color_space_raw);
		}

		const char* color_space_begin = reinterpret_cast<char*>(&color_space_raw);

		// small string optimization, hopefully
		return std::string(color_space_begin, sizeof(color_space_raw));
	}

	color_space get_ColorSpaceType() const {
		color_space result = static_cast<color_space>(this->get_unsigned<name_to_index("ColorSpaceType"sv)>());

		constexpr std::array known_color_spaces{
			color_space::calibrated_rgb,
			color_space::sRGB,
			color_space::windows
		};

		auto it = std::find(known_color_spaces.cbegin(), known_color_spaces.cend(), result);
		if (it == known_color_spaces.cend()) {
			result = color_space::unknown;
		}

		return result;
	}

	std::array<uint32_t, 9> get_ColorSpaceEndPoints() const {
		return {
			this->get_unsigned<name_to_index("ColorSpaceEndPoint_001"sv)>(),
			this->get_unsigned<name_to_index("ColorSpaceEndPoint_002"sv)>(),
			this->get_unsigned<name_to_index("ColorSpaceEndPoint_003"sv)>(),
			this->get_unsigned<name_to_index("ColorSpaceEndPoint_004"sv)>(),
			this->get_unsigned<name_to_index("ColorSpaceEndPoint_005"sv)>(),
			this->get_unsigned<name_to_index("ColorSpaceEndPoint_006"sv)>(),
			this->get_unsigned<name_to_index("ColorSpaceEndPoint_007"sv)>(),
			this->get_unsigned<name_to_index("ColorSpaceEndPoint_008"sv)>(),
			this->get_unsigned<name_to_index("ColorSpaceEndPoint_009"sv)>()
		};
	}

	uint32_t get_GammaChannel1() const {
		return this->get_unsigned<name_to_index("GammaChannel1"sv)>();
	}

	uint32_t get_GammaChannel2() const {
		return this->get_unsigned<name_to_index("GammaChannel2"sv)>();
	}

	uint32_t get_GammaChannel3() const {
		return this->get_unsigned<name_to_index("GammaChannel3"sv)>();
	}

	std::array<uint32_t, 3> get_ChannelGammas() const {
		return { 
			this->get_unsigned<name_to_index("GammaChannel1"sv)>(),
			this->get_unsigned<name_to_index("GammaChannel2"sv)>(),
			this->get_unsigned<name_to_index("GammaChannel3"sv)>(),
		};
	}

	void set_ColorSpaceType(color_space value) {
		return this->set_unsigned<name_to_index("ColorSpaceType"sv)>(to_underlying(value));
	}

	void set_GammaChannel1(uint32_t value) {
		return this->set_unsigned<name_to_index("GammaChannel1"sv)>(value);
	}

	void set_GammaChannel2(uint32_t value) {
		return this->set_unsigned<name_to_index("GammaChannel2"sv)>(value);
	}

	void set_GammaChannel3(uint32_t value) {
		return this->set_unsigned<name_to_index("GammaChannel3"sv)>(value);
	}

	void set_ChannelGammas(const std::array<uint32_t, 3>& values) {
		this->set_unsigned<name_to_index("GammaChannel1"sv)>(values[0]);
		this->set_unsigned<name_to_index("GammaChannel2"sv)>(values[1]);
		this->set_unsigned<name_to_index("GammaChannel3"sv)>(values[2]);
	}

	// no need for setting color space end points is obvious as for now, therefore no interface provided

	const std::array<std::byte, bitfield::size>& commit() {
		return bitfield::commit();
	}

	static constexpr size_t size() {
		return bitfield::size;
	}
};


class bitmap_info_v5_header;

template<>
struct bitfield_traits<bitmap_info_v5_header> {
	static constexpr fields_description_t fields{
		bitfield_description{"Intent"sv,		{0, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"IccOffset"sv,		{32, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"IccSize"sv,		{64, 32}, (uint32_t)(0)}, 	// 
		bitfield_description{"reserved_003"sv,	{96, 32}, (uint32_t)(0)}, 	//
	};	// 16 bytes
};


class bitmap_info_v5_header : private bitfield<bitmap_info_v5_header> {
	static_assert(bitfield::size == 16);

private:
	template <size_t field_index>
	auto get_unsigned() const {
		return byteswap(this->get_bitfield<field_index>());
	}

	template <size_t field_index>
	void set_unsigned(auto value) {
		using value_t = std::invoke_result_t<decltype(&bitfield::get_bitfield<field_index>), const bitfield>;
		this->set_bitfield<field_index>(byteswap((value_t)(value)));
	}

public:
	bitmap_info_v5_header() = default;
	bitmap_info_v5_header(std::span<const std::byte, bitfield::size> raw_data) : bitfield(raw_data) { }

	uint32_t get_Intent() const {
		return this->get_unsigned<name_to_index("Intent"sv)>();
	}

	uint32_t get_IccOffset() const {
		return this->get_unsigned<name_to_index("IccOffset"sv)>();
	}

	uint32_t get_IccSize() const {
		return this->get_unsigned<name_to_index("IccSize"sv)>();
	}

	void set_Intent(uint32_t value) {
		return this->set_unsigned<name_to_index("Intent"sv)>(value);
	}

	// no need for setting ICC info is obvious as for now, therefore no interface provided

	const std::array<std::byte, bitfield::size>& commit() {
		this->set_default<name_to_index("reserved_003"sv)>();

		return bitfield::commit();
	}

	static constexpr size_t size() {
		return bitfield::size;
	}
};
