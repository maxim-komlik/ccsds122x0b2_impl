#pragma once

#include <array>
#include <tuple>
#include <string_view>
#include <utility>

#include "core_types.hpp"
#include "utils.hpp"

#include "bitfield.hpp"
#include "constant.hpp"

struct HeaderPart_1A;

template<>
struct bitfield_traits<HeaderPart_1A> {
	static constexpr fields_description_t fields {
		bitfield_description{"StartImgFlag"sv,	{0, 1}, false},		// Flags initial segment in an image
		bitfield_description{"EndImgFlag"sv,	{1, 1}, false},		// Flags final segment in an image
		bitfield_description{"SegmentCount"sv,	{2, 8}, 1u},		// Segment counter value
		bitfield_description{"BitDepthDC"sv,	{10, 5}, 0u},		// Number of bits needed to represent DC coefficients in 2’s complement representation
		bitfield_description{"BitDepthAC"sv,	{15, 5}, 0u},		// Number of bits needed to represent absolute value of AC coefficients in unsigned integer representation
		bitfield_description{"reserved_001"sv,	{20, 1}, 0u},		// 1 bit reserved
		bitfield_description{"Part2Flag"sv,		{21, 1}, false},	// Indicates presence of Part 2 header
		bitfield_description{"Part3Flag"sv,		{22, 1}, false},	// Indicates presence of Part 3 header
		bitfield_description{"Part4Flag"sv,		{23, 1}, false}		// Indicates presence of Part 4 header
	};	// 24 bits = 3 bytes
};


// Table 4-4, Page 4-7
struct HeaderPart_1A : private bitfield<HeaderPart_1A> {
	static_assert(bitfield<HeaderPart_1A>::size == 3);
	// TODO: better implement composition with bitfield instead of inheritance
public:
	HeaderPart_1A() = default;
	HeaderPart_1A(std::span<std::byte, bitfield::size> raw_data) : bitfield(raw_data) {
		bool valid = true;
		if constexpr (dbg::protocol::if_disabled(dbg::protocol::mask_forward_compatibility)) {
			valid &= (this->get_bitfield<name_to_index("reserved_001"sv)>() == 
				this->get_default<name_to_index("reserved_001"sv)>());
		}

		if (!valid) {
			// TODO: error handling
		}
	};

	void set_StartImgFlag(bool value) {
		this->set_bitfield<name_to_index("StartImgFlag"sv)>((uint8_t)value);
	}

	void set_EndImgFlag(bool value) {
		this->set_bitfield<name_to_index("EndImgFlag"sv)>((uint8_t)value);
	}

	void set_SegmentCount(size_t value) {
		this->set_bitfield<name_to_index("SegmentCount"sv)>((uint8_t)value);
	}

	void set_BitDepthDC(size_t value) {
		this->set_bitfield<name_to_index("BitDepthDC"sv)>((uint8_t)value);
	}

	void set_BitDepthAC(size_t value) {
		this->set_bitfield<name_to_index("BitDepthAC"sv)>((uint8_t)value);
	}

	void set_Part2Flag(bool value) {
		this->set_bitfield<name_to_index("Part2Flag"sv)>((uint8_t)value);
	}

	void set_Part3Flag(bool value) {
		this->set_bitfield<name_to_index("Part3Flag"sv)>((uint8_t)value);
	}

	void set_Part4Flag(bool value) {
		this->set_bitfield<name_to_index("Part4Flag"sv)>((uint8_t)value);
	}

	bool get_StartImgFlag() const {
		return this->get_bitfield<name_to_index("StartImgFlag"sv)>();
	}

	bool get_EndImgFlag() const {
		return this->get_bitfield<name_to_index("EndImgFlag"sv)>();
	}

	size_t get_SegmentCount() const {
		return this->get_bitfield<name_to_index("SegmentCount"sv)>();
	}

	size_t get_BitDepthDC() const {
		return this->get_bitfield<name_to_index("BitDepthDC"sv)>();
	}

	size_t get_BitDepthAC() const {
		return this->get_bitfield<name_to_index("BitDepthAC"sv)>();
	}

	bool get_Part2Flag() const {
		return this->get_bitfield<name_to_index("Part2Flag"sv)>();
	}

	bool get_Part3Flag() const {
		return this->get_bitfield<name_to_index("Part3Flag"sv)>();
	}

	bool get_Part4Flag() const {
		return this->get_bitfield<name_to_index("Part4Flag"sv)>();
	}

	const bitfield_buffer_t& commit() {
		set_default<name_to_index("reserved_001"sv)>();
		return bitfield<HeaderPart_1A>::commit();
	}

	static constexpr size_t size() {
		return bitfield<HeaderPart_1A>::size;
	}
};


struct HeaderPart_1B;

template<>
struct bitfield_traits<HeaderPart_1B> {
	static constexpr fields_description_t fields {
		bitfield_description{"PadRows"sv,		{0, 3}, 0u},		// Number of ‘padding’ rows to delete after inverse DWT
		bitfield_description{"reserved_001"sv,	{3, 5}, 0u},		// 5 bits reserved
	};	// 8 bits = 1 byte
};

// Table 4-4, Page 4-7
struct HeaderPart_1B : bitfield<HeaderPart_1B> {
	static_assert(bitfield<HeaderPart_1B>::size == 1);

	HeaderPart_1B() = default;
	HeaderPart_1B(std::span<std::byte, bitfield::size> raw_data) : bitfield(raw_data) {
		bool valid = true;
		if constexpr (dbg::protocol::if_disabled(dbg::protocol::mask_forward_compatibility)) {
			valid &= (this->get_bitfield<name_to_index("reserved_001"sv)>() == 
				this->get_default<name_to_index("reserved_001"sv)>());
		}

		if (!valid) {
			// TODO: error handling
		}
	};

	void set_PadRows(size_t rows_num) {
		this->set_bitfield<name_to_index("PadRows"sv)>((uint8_t)rows_num);
	}

	size_t get_PadRows() const {
		return this->get_bitfield<name_to_index("PadRows"sv)>();
	}

	const bitfield_buffer_t& commit() {
		set_default<name_to_index("reserved_001"sv)>();
		return bitfield<HeaderPart_1B>::commit();
	}

	static constexpr size_t size() {
		return bitfield<HeaderPart_1B>::size;
	}
};


struct HeaderPart_2;

template<>
struct bitfield_traits<HeaderPart_2> {
	static constexpr fields_description_t fields {
		bitfield_description{"SegByteLimit"sv,	{0, 27}, 0u},		// Maximum number of bytes in a coded segment
		bitfield_description{"DCStop"sv,		{27, 1}, false},	// Indicates whether coded segment stops after coding of quantized DC coefficients(4.3)
		bitfield_description{"BitPlaneStop"sv,	{28, 5}, 0u},		// Unused when DCStop = 1. 
			// When DCStop = 0, indicates limit on coding of DWT coefficient bit
			// planes. When BitPlaneStop = b and StageStop = s, coded segment 
			// terminates once stage s of bit plane b has been completed(see 4.5),
			// unless coded segment terminates earlier because of the coded segment 
			// byte limit(SegByteLimit)
		bitfield_description{"StageStop"sv,		{33, 2}, 0b11u},	// No description in source, codes stage 1/2/3/4 : 00/01/10/11
		bitfield_description{"UseFill"sv,		{35, 1}, false},	// Specifies whether fill bits will be used to produce SegByteLimit bytes in each coded segment
		bitfield_description{"reserved_001"sv,	{36, 4}, 0u}		// 4 bits reserved
	};	// 40 bits = 5 bytes
};

// Table 4-5, Page 4-10
struct HeaderPart_2 : bitfield<HeaderPart_2> {
	static_assert(bitfield<HeaderPart_2>::size == 5);

	HeaderPart_2() = default;
	HeaderPart_2(std::span<std::byte, bitfield::size> raw_data) : bitfield(raw_data) {
		constexpr size_t min_segment_size_bytes = 9;

		bool valid = true;
		valid &= (this->get_bitfield<name_to_index("SegByteLimit"sv)>() >= min_segment_size_bytes);
		valid &= !(
			(this->get_bitfield<name_to_index("DCStop"sv)>() == true) & 
			(this->get_bitfield<name_to_index("BitPlaneStop"sv)>() != 
				this->get_default<name_to_index("BitPlaneStop"sv)>()));

		if constexpr (dbg::protocol::if_disabled(dbg::protocol::mask_forward_compatibility)) {
			valid &= (this->get_bitfield<name_to_index("reserved_001"sv)>() == 
				this->get_default<name_to_index("reserved_001"sv)>());
		}

		if (!valid) {
			// TODO: error handling
		}
	};

	void set_SegByteLimit(size_t byte_limit) {
		this->set_bitfield<name_to_index("SegByteLimit"sv)>((uint32_t)byte_limit);
	}

	void set_DCStop(bool value) {
		this->set_bitfield<name_to_index("DCStop"sv)>((uint8_t)value);
	}

	void set_BitPlaneStop(size_t bplane_index) {
		this->set_bitfield<name_to_index("BitPlaneStop"sv)>((uint8_t)bplane_index);
	}

	void set_StageStop(size_t stage_index) {
		this->set_bitfield<name_to_index("StageStop"sv)>((uint8_t)stage_index);
	}

	void set_UseFill(bool value) {
		this->set_bitfield<name_to_index("UseFill"sv)>((uint8_t)value);
	}

	size_t get_SegByteLimit() const {
		return this->get_bitfield<name_to_index("SegByteLimit"sv)>();
	}

	bool get_DCStop() const {
		return this->get_bitfield<name_to_index("DCStop"sv)>();
	}

	size_t get_BitPlaneStop() const {
		return this->get_bitfield<name_to_index("BitPlaneStop"sv)>();
	}

	size_t get_StageStop() const {
		return this->get_bitfield<name_to_index("StageStop"sv)>();
	}

	bool get_UseFill() const {
		return this->get_bitfield<name_to_index("UseFill"sv)>();
	}

	const bitfield_buffer_t& commit() {
		set_default<name_to_index("reserved_001"sv)>();
		return bitfield<HeaderPart_2>::commit();
	}

	static constexpr size_t size() {
		return bitfield<HeaderPart_2>::size;
	}
};


struct HeaderPart_3;

template<>
struct bitfield_traits<HeaderPart_3> {
	static constexpr fields_description_t fields {
		bitfield_description{"S"sv,				{0, 20}, 0u},		// segment size in blocks
		bitfield_description{"OptDCSelect"sv,	{20, 1}, true},		// Specifies whether optimum or heuristic method is used 
			// to select value of k parameter for coding quantized DC coefficient 
			// values (see 4.3.2)
		bitfield_description{"OptACSelect"sv,	{21, 1}, true},		// Specifies whether optimum or heuristic method is used 
			// to select value of k parameter for coding BitDepthAC (see 4.4)
		bitfield_description{"reserved_001"sv,	{22, 2}, 0u}		// 2 bits reserved
	};	// 24 bits = 3 bytes
};

// Table 4-6, Page 4-13
struct HeaderPart_3 : bitfield<HeaderPart_3> {
	static_assert(bitfield<HeaderPart_3>::size == 3);

	HeaderPart_3() = default;
	HeaderPart_3(std::span<std::byte, bitfield::size> raw_data) : bitfield(raw_data) {
		bool valid = true;
		if constexpr (dbg::protocol::if_disabled(dbg::protocol::mask_forward_compatibility)) {
			valid &= (this->get_bitfield<name_to_index("reserved_001"sv)>() == 
				this->get_default<name_to_index("reserved_001"sv)>());
		}

		if (!valid) {
			// TODO: error handling
		}
	};

	void set_S(size_t blocks_per_segment) {
		this->set_bitfield<name_to_index("S"sv)>((uint32_t)blocks_per_segment);
	}

	void set_OptDCSelect(bool value) {
		this->set_bitfield<name_to_index("OptDCSelect"sv)>((uint8_t)value);
	}

	void set_OptACSelect(bool value) {
		this->set_bitfield<name_to_index("OptACSelect"sv)>((uint8_t)value);
	}

	size_t get_S() const {
		return this->get_bitfield<name_to_index("S"sv)>();
	}

	bool get_OptDCSelect() const {
		return this->get_bitfield<name_to_index("OptDCSelect"sv)>();
	}

	bool get_OptACSelect() const {
		return this->get_bitfield<name_to_index("OptACSelect"sv)>();
	}

	const bitfield_buffer_t& commit() {
		set_default<name_to_index("reserved_001"sv)>();
		return bitfield<HeaderPart_3>::commit();
	}

	static constexpr size_t size() {
		return bitfield<HeaderPart_3>::size;
	}
};


struct HeaderPart_4;

template<>
struct bitfield_traits<HeaderPart_4> {
	static constexpr fields_description_t fields {
		bitfield_description{"DWTtype"sv,		{0, 1}, 1u},		// Specifies DWT type	
		bitfield_description{"reserved_001"sv,	{1, 1}, 0u},		// 1 bit reserved
		bitfield_description{"ExtendedPixelBitDepthFlag"sv, {2, 1}, false}, // Indicates an input pixel bit depth larger than 16
		bitfield_description{"SignedPixels"sv,	{3, 1}, false},		// Specifies whether input pixel values are signed or unsigned quantities
		bitfield_description{"PixelBitDepth"sv, {4, 4}, 1u},		// Together with ExtendedPixelBitDepth Flag, indicates the input pixel bit depth
		bitfield_description{"ImageWidth"sv,	{8, 20}, 0u},		// Image width in pixels
		bitfield_description{"TransposeImg"sv,	{28, 1}, false},	// Indicates whether entire image should be transposed after reconstruction
		bitfield_description{"CodeWordLength"sv, {29, 3}, (uint8_t)get_codeword_length_value(sizeof(size_t) << 3)}, // Indicates the coded word length
		bitfield_description{"CustomWtFlag"sv,	{32, 1}, false},	// Indicates if weights in 3.9 used or user defined		
			// Subband weights: {00, 01, 10, 11} => {2^0, 2^1, 2^2, 2^3}
		bitfield_description{"CustomWtHH__1"sv, {33, 2}, 0u},		// Weight of HH1 subband
		bitfield_description{"CustomWtHL__1"sv, {35, 2}, 0u},		// Weight of HL1 subband
		bitfield_description{"CustomWtLH__1"sv, {37, 2}, 0u},		// Weight of LH1 subband
		bitfield_description{"CustomWtHH__2"sv, {39, 2}, 0u},		// Weight of HH2 subband
		bitfield_description{"CustomWtHL__2"sv, {41, 2}, 0u},		// Weight of HL2 subband
		bitfield_description{"CustomWtLH__2"sv, {43, 2}, 0u},		// Weight of LH2 subband
		bitfield_description{"CustomWtHH__3"sv, {45, 2}, 0u},		// Weight of HH3 subband
		bitfield_description{"CustomWtHL__3"sv, {47, 2}, 0u},		// Weight of HL3 subband
		bitfield_description{"CustomWtLH__3"sv, {49, 2}, 0u},		// Weight of LH3 subband
		bitfield_description{"CustomWtLL__3"sv, {51, 2}, 0u},		// Weight of LL3 subband
		bitfield_description{"reserved_002"sv,	{53, 11}, 0u}		// 11 bits reserved
	};	// 64 bits = 8 bytes
};

// Table 4-7, Page 4-15
struct HeaderPart_4 : bitfield<HeaderPart_4> {
	static_assert(bitfield<HeaderPart_4>::size == 8);

	HeaderPart_4() = default;
	HeaderPart_4(std::span<std::byte, bitfield::size> raw_data) : bitfield(raw_data) {
		bool valid = true;
		valid &= !(
			(this->get_bitfield<name_to_index("CustomWtFlag"sv)>() == false) &
			(
				(this->get_bitfield<name_to_index("CustomWtHH__1"sv)>() !=
					this->get_default<name_to_index("CustomWtHH__1"sv)>()) &
				(this->get_bitfield<name_to_index("CustomWtHL__1"sv)>() !=
					this->get_default<name_to_index("CustomWtHL__1"sv)>()) &
				(this->get_bitfield<name_to_index("CustomWtLH__1"sv)>() !=
					this->get_default<name_to_index("CustomWtLH__1"sv)>()) &
				(this->get_bitfield<name_to_index("CustomWtHH__2"sv)>() !=
					this->get_default<name_to_index("CustomWtHH__2"sv)>()) &
				(this->get_bitfield<name_to_index("CustomWtHL__2"sv)>() !=
					this->get_default<name_to_index("CustomWtHL__2"sv)>()) &
				(this->get_bitfield<name_to_index("CustomWtLH__2"sv)>() !=
					this->get_default<name_to_index("CustomWtLH__2"sv)>()) &
				(this->get_bitfield<name_to_index("CustomWtHH__3"sv)>() !=
					this->get_default<name_to_index("CustomWtHH__3"sv)>()) &
				(this->get_bitfield<name_to_index("CustomWtHL__3"sv)>() !=
					this->get_default<name_to_index("CustomWtHL__3"sv)>()) &
				(this->get_bitfield<name_to_index("CustomWtLH__3"sv)>() !=
					this->get_default<name_to_index("CustomWtLH__3"sv)>()) &
				(this->get_bitfield<name_to_index("CustomWtLL__3"sv)>() !=
					this->get_default<name_to_index("CustomWtLL__3"sv)>())));

		if constexpr (dbg::protocol::if_disabled(dbg::protocol::mask_forward_compatibility)) {
			valid &= (this->get_bitfield<name_to_index("reserved_001"sv)>() == 
				this->get_default<name_to_index("reserved_001"sv)>());
			valid &= (this->get_bitfield<name_to_index("reserved_002"sv)>() == 
				this->get_default<name_to_index("reserved_002"sv)>());
		}

		if (!valid) {
			// TODO: error handling
		}
	};

	void set_DWTtype(dwt_type_t value) {
		this->set_bitfield<name_to_index("DWTtype"sv)>((uint8_t)value);
	}

	void set_SignedPixels(bool if_signed) {
		this->set_bitfield<name_to_index("SignedPixels"sv)>((uint8_t)if_signed);
	}

	void set_PixelBitDepth(size_t bdepth) {
		constexpr size_t bdepth_basic = 4;
		constexpr size_t extended_threshold = 1 << bdepth_basic;
		constexpr size_t bdepth_mask = extended_threshold - 1;

		bool extended = bdepth > extended_threshold;
		this->set_bitfield<name_to_index("ExtendedPixelBitDepthFlag"sv)>((uint8_t)extended);
		this->set_bitfield<name_to_index("PixelBitDepth"sv)>((uint8_t)(bdepth & bdepth_mask));
	}

	void set_ImageWidth(size_t width) {
		this->set_bitfield<name_to_index("ImageWidth"sv)>((uint32_t)width);
	}

	void set_TransposeImg(bool value) {
		this->set_bitfield<name_to_index("TransposeImg"sv)>((uint8_t)value);
	}

	void set_CustomWtFlag(bool value) {
		if (!value & get_CustomWtFlag()) {
			constexpr size_t default_weight = 0;
			this->set_bitfield<name_to_index("CustomWtHH__1"sv)>((uint8_t)default_weight);
			this->set_bitfield<name_to_index("CustomWtHL__1"sv)>((uint8_t)default_weight);
			this->set_bitfield<name_to_index("CustomWtLH__1"sv)>((uint8_t)default_weight);
			this->set_bitfield<name_to_index("CustomWtHH__2"sv)>((uint8_t)default_weight);
			this->set_bitfield<name_to_index("CustomWtHL__2"sv)>((uint8_t)default_weight);
			this->set_bitfield<name_to_index("CustomWtLH__2"sv)>((uint8_t)default_weight);
			this->set_bitfield<name_to_index("CustomWtHH__3"sv)>((uint8_t)default_weight);
			this->set_bitfield<name_to_index("CustomWtHL__3"sv)>((uint8_t)default_weight);
			this->set_bitfield<name_to_index("CustomWtLH__3"sv)>((uint8_t)default_weight);
			this->set_bitfield<name_to_index("CustomWtLL__3"sv)>((uint8_t)default_weight);
		}
		this->set_bitfield<name_to_index("CustomWtFlag"sv)>((uint8_t)value);
	}

	void set_CustomWt(shifts_t shifts) {
		constexpr shifts_t default_shifts{ 3, 3, 3, 2, 2, 2, 1, 1, 1, 0 };
		if (shifts == default_shifts) {
			// default values for shifts provided, disable custom shifts
			this->set_CustomWtFlag(false);
		}
		// LL3, HL3, LH3, HH3, HL2, LH2, HH2, HL1, LH1, HH1
		static constexpr std::array shift_fields_names{
			"CustomWtLL__3"sv, 
			"CustomWtHL__3"sv, 
			"CustomWtLH__3"sv, 
			"CustomWtHH__3"sv, 
			"CustomWtHL__2"sv, 
			"CustomWtLH__2"sv, 
			"CustomWtHH__2"sv, 
			"CustomWtHL__1"sv, 
			"CustomWtLH__1"sv, 
			"CustomWtHH__1"sv
		};

		std::apply([&](const auto& ... args) -> void {
				unroll<false, UnrollFoldType::left>::apply([&]<size_t index>(std::string_view) -> void {
						this->set_bitfield<name_to_index(std::get<index>(shift_fields_names))>((uint8_t)shifts[index]);
					}, args...);
			}, shift_fields_names);
	}

	dwt_type_t get_DWTtype() const {
		return static_cast<dwt_type_t>(this->get_bitfield<name_to_index("DWTtype"sv)>());
	}

	bool get_SignedPixels() const {
		return this->get_bitfield<name_to_index("SignedPixels"sv)>();
	}

	size_t get_PixelBitDepth() const {
		constexpr size_t bdepth_basic = 4;

		size_t result = (bool)(this->get_bitfield<name_to_index("ExtendedPixelBitDepthFlag"sv)>());
		result <<= bdepth_basic;
		return result + this->get_bitfield<name_to_index("PixelBitDepth"sv)>();
	}

	size_t get_ImageWidth() const {
		return this->get_bitfield<name_to_index("ImageWidth"sv)>();
	}

	bool get_TransposeImg() const {
		return this->get_bitfield<name_to_index("TransposeImg"sv)>();
	}

	codeword_length get_CodeWordLength() const {
		return static_cast<codeword_length>(this->get_bitfield<name_to_index("CodeWordLength"sv)>());
	}

	bool get_CustomWtFlag() const {
		return this->get_bitfield<name_to_index("CustomWtFlag"sv)>();
	}

	shifts_t get_CustomWt() const {
		shifts_t result;
		// TODO: static is not needed here as per the standard. 
		// However, it doesn't compile on msvc otherwise, seems to be msvc bug.
		static constexpr std::array shift_fields_names{
			"CustomWtLL__3"sv, 
			"CustomWtHL__3"sv, 
			"CustomWtLH__3"sv, 
			"CustomWtHH__3"sv, 
			"CustomWtHL__2"sv, 
			"CustomWtLH__2"sv, 
			"CustomWtHH__2"sv, 
			"CustomWtHL__1"sv, 
			"CustomWtLH__1"sv, 
			"CustomWtHH__1"sv
		};

		std::apply(
			[&](const auto& ... args) -> void {
				unroll<false, UnrollFoldType::left>::apply(
					[&]<size_t index>(std::string_view) -> void {
						result[index] = this->get_bitfield<name_to_index(shift_fields_names[index])>();
						// result[index] = this->get_bitfield<name_to_index(std::get<index>(shift_fields_names))>();
					}, args...);
			}, shift_fields_names);
		
		return result;
	}

	const bitfield_buffer_t& commit() {
		set_default<name_to_index("reserved_001"sv)>();
		set_default<name_to_index("reserved_002"sv)>();
		return bitfield<HeaderPart_4>::commit();
	}

	static constexpr size_t size() {
		return bitfield<HeaderPart_4>::size;
	}
};
