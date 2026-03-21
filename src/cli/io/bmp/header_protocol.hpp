#pragma once

#include <array>
#include <tuple>
#include <variant>
#include <span>
#include <utility>
#include <algorithm>
#include <cstddef>

#include "headers.hpp"
#include "dwt/img_meta.hpp"
#include "dwt/bitmap.tpp"

class bmp_header_protocol {
private:
	template <typename T>
	using header_record_t = std::pair<T, bool>;

	std::tuple<
		header_record_t<bitmap_file_header>, 
		// the two below are mutually exclusive
		header_record_t<
			std::variant<
				bitmap_core_v1_header, 
				bitmap_core_v2_header>>, 
		header_record_t<bitmap_info_v1_header>,
		header_record_t<bitmap_info_v2_header>,
		header_record_t<bitmap_info_v3_header>,
		header_record_t<bitmap_info_v4_header>,
		header_record_t<bitmap_info_v5_header>> headers;

	static constexpr size_t dib_header_index = 1;
	static constexpr size_t channel_num = 4;

public:
	bmp_header_protocol(std::span<const std::byte> raw_data);

	template <typename T>
	bmp_header_protocol(const std::vector<bitmap<T>>& img);

	bool if_contains_dib_header(dib_header_type header_type) const;

	img_meta get_image_description() const;
	img_pos get_image_scan_spec() const;
	std::array<uint32_t, 5> get_channel_masks() const;
	size_t get_pixel_bit_size() const;
	ptrdiff_t get_image_data_offset() const;

	size_t header_size() const;
	size_t file_size() const;

	std::span<std::byte> commit(std::span<std::byte> dst);

private:
	void init_from_image_specs(img_meta& target_specs, std::array<size_t, channel_num + 1>& channel_bdepths);

	size_t get_image_depth() const;;
	size_t dib_header_size() const;

private:
	static constexpr size_t dib_size_field_offset = 0;
	static constexpr size_t dib_size_field_size = sizeof(uint32_t);

public:
	static constexpr size_t header_preamble_size = bitmap_file_header::size() + 
		dib_size_field_offset + dib_size_field_size;

public:
	static constexpr size_t max_header_size() noexcept {
		return bitmap_file_header::size() + bitmap_core_v2_header::size() + 
			bitmap_info_v1_header::size() + 
			bitmap_info_v2_header::size() + 
			bitmap_info_v3_header::size() + 
			bitmap_info_v4_header::size() + 
			bitmap_info_v5_header::size(); 
		// corresponds to bitmap_file_header::size() + dib_header_type::bitmap_info_v5
	}

	static constexpr size_t min_header_size() noexcept { 
		return bitmap_file_header::size() + bitmap_core_v1_header::size(); 
	}

	static size_t hint_header_size(std::span<const std::byte, header_preamble_size> raw_data);
};


//
// implementation section

template <typename T>
bmp_header_protocol::bmp_header_protocol(const std::vector<bitmap<T>>& img) {
	constexpr size_t max_dimension_value = std::numeric_limits<int32_t>::max();	// height and width fields are 32-bit signed ints
	constexpr size_t max_depth = 32;

	bool valid = true;
	valid &= !img.empty();

	if (!valid) {
		// TODO: error handling, throw
	}

	img_meta target_specs = img.front().get_meta();
	target_specs.depth = img.size();

	auto it = std::find_if_not(img.cbegin(), img.cend(),
		[&target_specs](const auto& item) -> bool {
			bool result = true;
			result &= (item.get_meta().width == target_specs.width);
			result &= (item.get_meta().height == target_specs.height);

			return result;
		});


	valid &= (target_specs.depth > 0);
	valid &= (target_specs.depth <= channel_num);
	valid &= (target_specs.width <= max_dimension_value);
	valid &= (target_specs.height <= max_dimension_value);
	valid &= (target_specs.if_signed == false);

	if (!valid) {
		// TODO: error handling, throw
	}

	std::array<size_t, channel_num + 1> channel_bdepths = {};
	for (ptrdiff_t i = 0; i < target_specs.depth; ++i) {
		channel_bdepths[i] = bitmap_dynamic_bit_depth(img[i]);
	}

	// assume image channels are balanced, or the last channel is not balanced with respect to others
	// TODO: but ccsds implies only balanced channels, pixel bit depth is defined per session
	constexpr size_t default_bdepth = 8;
	size_t max_bdepth = *std::max_element(channel_bdepths.cbegin(), channel_bdepths.cend());
	size_t balanced_bdepth = std::max(max_bdepth, default_bdepth);

	bool last_unbalanced = (balanced_bdepth * target_specs.depth) > max_depth;

	valid &= (!last_unbalanced) | 
		(((balanced_bdepth * (target_specs.depth - 1)) + channel_bdepths[target_specs.depth - 1]) <= max_depth);

	if (!valid) {
		// TODO: error handling
	}

	for (ptrdiff_t i = 0; i < (target_specs.depth - last_unbalanced); ++i) {
		channel_bdepths[i] = balanced_bdepth;
	}

	init_from_image_specs(target_specs, channel_bdepths);
}


// template <typename T>
// bmp_header_protocol(const std::vector<bitmap<T>>& img) {
// 	constexpr size_t max_dimension_value = std::numeric_limits<int32_t>::max();	// height and width fields are 32-bit signed ints
// 	constexpr size_t max_depth = 32;
// 
// 	bool valid = true;
// 	valid &= !img.empty();
// 
// 	if (!valid) {
// 		// TODO: error handling, throw
// 	}
// 
// 	img_meta target_specs = img.front().get_meta();
// 	target_specs.depth = img.size();
// 
// 	auto it = std::find_if_not(img.cbegin(), img.cend(),
// 		[&target_specs](const auto& item) -> bool {
// 			bool result = true;
// 			result &= (item.get_meta().width == target_specs.width);
// 			result &= (item.get_meta().height == target_specs.height);
// 
// 			return result;
// 		});
// 
// 
// 	valid &= (target_specs.depth > 0);
// 	valid &= (target_specs.depth <= channel_num);
// 	valid &= (target_specs.width <= max_dimension_value);
// 	valid &= (target_specs.height <= max_dimension_value);
// 	valid &= (target_specs.if_signed == false);
// 
// 	if (!valid) {
// 		// TODO: error handling, throw
// 	}
// 
// 	std::array<size_t, channel_num + 1> channel_bdepths = {};
// 	for (ptrdiff_t i = 0; i < target_specs.depth; ++i) {
// 		channel_bdepths[i] = bitmap_dynamic_bit_depth(img[i]);
// 	}
// 
// 	// assume image channels are balanced, or the last channel is not balanced with respect to others
// 	// TODO: but ccsds implies only balanced channels, pixel bit depth is defined per session
// 	constexpr size_t default_bdepth = 8;
// 	size_t max_bdepth = *std::max_element(channel_bdepths.cbegin(), channel_bdepths.cend());
// 	size_t balanced_bdepth = std::max(max_bdepth, default_bdepth);
// 
// 	bool last_unbalanced = (balanced_bdepth * target_specs.depth) > max_depth;
// 
// 	valid &= (!last_unbalanced) | 
// 		(((balanced_bdepth * (target_specs.depth - 1)) + channel_bdepths[target_specs.depth - 1]) <= max_depth);
// 
// 	if (!valid) {
// 		// TODO: error handling
// 	}
// 
// 	for (ptrdiff_t i = 0; i < (target_specs.depth - last_unbalanced); ++i) {
// 		channel_bdepths[i] = balanced_bdepth;
// 	}
// 
// 	size_t pixel_depth = std::accumulate(channel_bdepths.cbegin(), channel_bdepths.cend(), 0);
// 
// 	auto& [header_dib, flag_dib] = std::get<dib_header_index>(this->headers);
// 	{
// 		bitmap_core_v2_header target_core_header;	
// 		// info v4 or v5 features are not used, so it would generally work fine with default v3, 
// 		// but v5 header is better recognized by other software
// 		target_core_header.set_HeaderSize(dib_header_type::bitmap_info_v5);
// 		target_core_header.set_ImageWidth(target_specs.width);
// 		target_core_header.set_ImageHeight(target_specs.height);	// scan image from bottom
// 		target_core_header.set_BitsPerPixel(pixel_depth);
// 		target_core_header.commit();
// 
// 		header_dib = target_core_header;
// 	}
// 
// 	{
// 		auto& [header, flag] = std::get<header_record_t<bitmap_info_v1_header>>(this->headers);
// 		header.set_Compression(bitmap_info_v1_header::compression_type::bitfields);
// 	}
// 
// 	size_t current_mask_shift = 0;
// 	auto make_subpixel_mask = [&current_mask_shift](size_t depth) -> uint32_t {
// 		uint32_t mask = (1 << depth) - 1;
// 		mask <<= current_mask_shift;
// 		current_mask_shift += depth;
// 		return mask;
// 	};
// 
// 	{
// 		auto& [header, flag] = std::get<header_record_t<bitmap_info_v3_header>>(this->headers);
// 		header.set_MaskChannel4(make_subpixel_mask(channel_bdepths[3]));
// 	}
// 
// 	{
// 		auto& [header, flag] = std::get<header_record_t<bitmap_info_v2_header>>(this->headers);
// 		header.set_MaskChannel3(make_subpixel_mask(channel_bdepths[2]));
// 		header.set_MaskChannel2(make_subpixel_mask(channel_bdepths[1]));
// 		header.set_MaskChannel1(make_subpixel_mask(channel_bdepths[0]));
// 	}
// 
// 	channel_bdepths.back() = pixel_depth - current_mask_shift;
// 
// 	constexpr size_t img_row_alignment_requirement = 4;
// 	size_t row_size = ((pixel_depth * target_specs.width) + ((1 << 3) - 1)) >> 3;
// 	row_size = (row_size + img_row_alignment_requirement - 1) & (~(img_row_alignment_requirement - 1));
// 
// 	size_t file_size = row_size * target_specs.height + this->header_size();	// no gaps assumed as for now
// 
// 	{
// 		auto& [header, flag] = std::get<header_record_t<bitmap_file_header>>(this->headers);
// 		header.set_ImageOffset(this->header_size());
// 		header.set_Size(file_size);
// 	}
// 
// 	// set default values not assigned explicitly above
// 	auto commit_header = [&]<typename hT>() -> void {
// 		auto& [header, flag] = std::get<header_record_t<hT>>(this->headers);
// 		const auto& content = header.commit();
// 	};
// 
// 	commit_header.template operator()<bitmap_file_header>();
// 	commit_header.template operator()<bitmap_info_v1_header>();
// 	commit_header.template operator()<bitmap_info_v2_header>();
// 	commit_header.template operator()<bitmap_info_v3_header>();
// 	commit_header.template operator()<bitmap_info_v4_header>();
// 	commit_header.template operator()<bitmap_info_v5_header>();
// 
// }
