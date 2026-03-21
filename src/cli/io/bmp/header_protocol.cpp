#pragma once

#include "header_protocol.hpp"

#include <functional>
#include <bit>
#include <limits>
#include <cmath>	// or <cstdlib> for std::abs?

#include "common/utility.hpp"

bmp_header_protocol::bmp_header_protocol(std::span<const std::byte> raw_data) {
	bool valid = true;
	valid &= (raw_data.size() >= min_header_size());

	if (!valid) {
		// TODO: error handling
		throw cli::bmp::header_invalid{};
	}


	auto& [header_file, flag_file] = std::get<header_record_t<bitmap_file_header>>(this->headers);
	header_file = bitmap_file_header(raw_data.subspan<0, bitmap_file_header::size()>());
	flag_file = true;

	raw_data = raw_data.subspan<bitmap_file_header::size()>();


	auto& [header_dib, flag_dib] = std::get<dib_header_index>(this->headers);
	try {
		// try core_v1 first; if failed, try core_v2
		header_dib = bitmap_core_v1_header(raw_data.subspan<0, bitmap_core_v1_header::size()>());
		raw_data = raw_data.subspan<bitmap_core_v1_header::size()>();
	} catch (const cli::bmp::header_invalid& e) {
		valid &= (raw_data.size() >= bitmap_core_v2_header::size());
		if (!valid) {
			// TODO: error handling, std::span contract violation, throw
			throw cli::bmp::header_invalid{};
		}

		header_dib = bitmap_core_v2_header(raw_data.subspan<0, bitmap_core_v2_header::size()>());
		raw_data = raw_data.subspan<bitmap_core_v2_header::size()>();
	}
	flag_dib = true;

	valid &= std::visit([&](const auto& h) -> bool {
				return (raw_data.size() >= (h.get_HeaderSize() - h.size()));
			},
			std::get<dib_header_index>(this->headers).first);
	if (!valid) {
		// TODO: error handling, std::span contract violation, throw
		throw cli::bmp::header_invalid{};
	}


	auto parse_header = [&]<typename hT>(bool if_present) -> void {
		if (if_present) {
			valid &= (raw_data.size() >= hT::size());
			if (!valid) {
				// TODO: error handling
			}

			auto& [header, flag] = std::get<header_record_t<hT>>(this->headers);
			header = hT(raw_data.subspan<0, hT::size()>());
			flag = true;

			raw_data = raw_data.subspan<hT::size()>();
		}
	};

	parse_header.template operator()<bitmap_info_v1_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v1));
	parse_header.template operator()<bitmap_info_v2_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v2));
	parse_header.template operator()<bitmap_info_v3_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v3));
	parse_header.template operator()<bitmap_info_v4_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v4));
	parse_header.template operator()<bitmap_info_v5_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v5));

	// validate complete header
	//
	valid &= std::visit([](const auto& h) -> bool {
			// have no idea how to handle multi-plane images, so it is not supported.
			// and wikipedia suggests that this value must be 1 (for info_v1 header at least, maybe)
			return h.get_PlaneCount() == 1;	
		}, 
		std::get<dib_header_index>(this->headers).first);

	if (this->if_contains_dib_header(dib_header_type::bitmap_info_v1)) {
		auto& [header, flag] = std::get<header_record_t<bitmap_info_v1_header>>(this->headers);

		using compression_type = bitmap_info_v1_header::compression_type;
			
		constexpr std::array supported_compression_types = std::invoke([]() constexpr {
				// support only uncompressed bitmaps, reject any embedded compression method
				compression_type values[] {
					compression_type::rgb,
					compression_type::cmyk,
					// not sure about options below:
					// 
					compression_type::bitfields,
					compression_type::alpha_bitfields,
				};
				return std::to_array(values);
			});

		valid &= (supported_compression_types.cend() !=
			std::find(supported_compression_types.cbegin(), supported_compression_types.cend(),
				header.get_Compression()));
		// color table is considered compression technique, therefore don't allow it
		valid &= (header.get_ColorTableItemCount() == 0);
	}

	if (!valid) {
		// TODO: throw 
		throw cli::bmp::header_invalid{};
	}
}

void bmp_header_protocol::init_from_image_specs(
		img_meta& target_specs, std::array<size_t, channel_num + 1>& channel_bdepths) {
	size_t pixel_depth = std::accumulate(channel_bdepths.cbegin(), channel_bdepths.cend(), 0);

	{
		auto& [header_dib, flag_dib] = std::get<dib_header_index>(this->headers);

		bitmap_core_v2_header target_core_header;	
		// info v4 or v5 features are not used, so it would generally work fine with default v3, 
		// but v5 header is better recognized by other software
		target_core_header.set_HeaderSize(dib_header_type::bitmap_info_v5);
		target_core_header.set_ImageWidth(target_specs.width);
		target_core_header.set_ImageHeight(target_specs.height);	// scan image from bottom
		target_core_header.set_BitsPerPixel(pixel_depth);
		target_core_header.commit();

		header_dib = target_core_header;
	}

	{
		auto& [header, flag] = std::get<header_record_t<bitmap_info_v1_header>>(this->headers);
		header.set_Compression(bitmap_info_v1_header::compression_type::bitfields);
	}

	size_t current_mask_shift = 0;
	auto make_subpixel_mask = [&current_mask_shift](size_t depth) -> uint32_t {
		uint32_t mask = (1 << depth) - 1;
		mask <<= current_mask_shift;
		current_mask_shift += depth;
		return mask;
	};

	{
		auto& [header, flag] = std::get<header_record_t<bitmap_info_v3_header>>(this->headers);
		header.set_MaskChannel4(make_subpixel_mask(channel_bdepths[3]));
	}

	{
		auto& [header, flag] = std::get<header_record_t<bitmap_info_v2_header>>(this->headers);
		header.set_MaskChannel3(make_subpixel_mask(channel_bdepths[2]));
		header.set_MaskChannel2(make_subpixel_mask(channel_bdepths[1]));
		header.set_MaskChannel1(make_subpixel_mask(channel_bdepths[0]));
	}

	channel_bdepths.back() = pixel_depth - current_mask_shift;

	constexpr size_t img_row_alignment_requirement = 4;
	size_t row_size = ((pixel_depth * target_specs.width) + ((1 << 3) - 1)) >> 3;
	row_size = (row_size + img_row_alignment_requirement - 1) & (~(img_row_alignment_requirement - 1));

	size_t file_size = row_size * target_specs.height + this->header_size();	// no gaps assumed as for now

	{
		auto& [header, flag] = std::get<header_record_t<bitmap_file_header>>(this->headers);
		header.set_ImageOffset(this->header_size());
		header.set_Size(file_size);
	}

	// set default values not assigned explicitly above
	auto commit_header = [&]<typename hT>() -> void {
		auto& [header, flag] = std::get<header_record_t<hT>>(this->headers);
		const auto& content = header.commit();
	};

	commit_header.template operator()<bitmap_file_header>();
	commit_header.template operator()<bitmap_info_v1_header>();
	commit_header.template operator()<bitmap_info_v2_header>();
	commit_header.template operator()<bitmap_info_v3_header>();
	commit_header.template operator()<bitmap_info_v4_header>();
	commit_header.template operator()<bitmap_info_v5_header>();
}

bool bmp_header_protocol::if_contains_dib_header(dib_header_type header_type) const {
	return std::visit(overloaded_callable{
			[header_type](const bitmap_core_v1_header& h) -> bool {
				std::array known_headers = std::invoke([]() constexpr {
						dib_header_type values[]{
							dib_header_type::bitmap_core_v1
						};
						return std::to_array(values);
					});
				return std::find(known_headers.cbegin(), known_headers.cend(), header_type) != known_headers.cend();
			},
			[header_type](const bitmap_core_v2_header& h) -> bool {
				std::array known_headers = std::invoke([]() constexpr {
						dib_header_type values[]{	// sorted
							dib_header_type::bitmap_core_v2,
							dib_header_type::bitmap_info_v1,
							dib_header_type::bitmap_info_v2,
							dib_header_type::bitmap_info_v3,
							dib_header_type::bitmap_info_v4,
							dib_header_type::bitmap_info_v5
						};
						return std::to_array(values);
					});

				bool result = true;
				result &= (std::find(known_headers.cbegin(), known_headers.cend(), header_type) != 
					known_headers.cend());
				result &= (header_type <= static_cast<dib_header_type>(h.get_HeaderSize()));

				return result;
			}
		}, 
		std::get<dib_header_index>(this->headers).first);
}

img_meta bmp_header_protocol::get_image_description() const {
	img_meta result{};

	std::tie(result.width, result.height) = std::visit(
		[](const auto& h) -> std::pair<size_t, size_t> {
			return { std::abs(h.get_ImageWidth()), std::abs(h.get_ImageHeight()) };
		}, 
		std::get<dib_header_index>(this->headers).first);
		
	result.depth = this->get_image_depth();

	auto masks = this->get_channel_masks();
	std::span significant_masks = std::span(masks).first<std::tuple_size_v<decltype(masks)> - 1>();
	auto max_it = std::max_element(significant_masks.begin(), significant_masks.end(),
		[](uint32_t lhs, uint32_t rhs) -> bool { return std::popcount(lhs) < std::popcount(rhs); });
	result.bdepth_static = std::popcount(*max_it);
		
	result.alignment_requirement = sizeof(uint32_t);
	result.if_signed = false; // bmp pixels are never signed

	return result;
}

img_pos bmp_header_protocol::get_image_scan_spec() const {
	// in BMP protocol, image width and height are signed values, and image origin is assumed 
	// to be in the left bottom corner, that is, by default the image is described 
	// bottom-to-up, left-to-right. Negative image dimension values are used to denote 
	// different image origin location: negative height places origin in the top-left corner, 
	// that is, traditional image description top-to-bottom, left-to-right.
		
	img_pos result{};


	std::tie(result.x, result.y) = std::visit(
		[](const auto& h) -> std::pair<ptrdiff_t, ptrdiff_t> {
			return { h.get_ImageWidth(), h.get_ImageHeight() };
		},
		std::get<dib_header_index>(this->headers).first);

	// internal project image representation (as used in bitmap class) is top-to-bottom, 
	// left-to-right. Map BMP scan order to internal scan order so that encoded BMP 
	// origin location is projected to internal bitmap top-to-bottom, left-to-right

	result.y = -result.y;

	ptrdiff_t x_sign = (result.x >> ((sizeof(result.x) << 3) - 1));
	ptrdiff_t y_sign = (result.y >> ((sizeof(result.y) << 3) - 1));

	result.x_step = (result.x >> ((sizeof(result.x) << 3) - 1)) | 0x01;
	result.y_step = (result.y >> ((sizeof(result.y) << 3) - 1)) | 0x01;
	result.width = std::abs(result.x);
	result.height = std::abs(result.y);

	// maps to 0 or [dimension - 1]
	result.x = signxor(result.x) & x_sign;
	result.y = signxor(result.y) & y_sign;

	// no transformations for z axis/depth
	result.z = 0;
	result.depth = this->get_image_depth();
	result.z_step = 1;

	result.x_stride = 1;
	result.y_stride = result.width;
	result.z_stride = result.width * result.height;

	return result;
}

std::array<uint32_t, 5> bmp_header_protocol::get_channel_masks() const {
	// returns array of masks. 
	// The first 4 elements represent masks for 4 channels, value 0 is used if the channel 
	// is not present in the image.
	// The last element represent X mask used to denote unused bits; that is, that bits 
	// should be accounted for bit padding between subsequent pixels.

	using result_t = std::array<uint32_t, channel_num + 1>;

	overloaded_callable visitor{
		[this](const bitmap_core_v1_header& h) -> result_t {
			return {
				(1u << this->get_pixel_bit_size()) - 1u,
				0
			};
		},
		[this](const bitmap_core_v2_header& h) -> result_t {
			result_t result = { 0 };

			size_t pixel_bit_size = this->get_pixel_bit_size();

			if (this->if_contains_dib_header(dib_header_type::bitmap_info_v2)) {
				const auto& [header, flag] = std::get<header_record_t<bitmap_info_v2_header>>(this->headers);
				result[0] = header.get_MaskChannel1();
				result[1] = header.get_MaskChannel2();
				result[2] = header.get_MaskChannel3();
			}

			if (this->if_contains_dib_header(dib_header_type::bitmap_info_v3)) {
				const auto& [header, flag] = std::get<header_record_t<bitmap_info_v3_header>>(this->headers);
				result[3] = header.get_MaskChannel4();
			}

			auto it = std::find_if(result.cbegin(), result.cend(), [](auto item) -> bool { return item != 0; });
			if (it == result.cend()) {
				// no masks set, initialize to some kind of default depending on compression
				const auto& [header, flag] = std::get<header_record_t<bitmap_info_v1_header>>(this->headers);
				size_t channel_num = 0;

				switch (header.get_Compression()) {
					// seems like those below can be used interchangebly (Windows Meta File docs specifies only 
					// the first) meaning that info_v2 and info_v3 parameters should be used
					case bitmap_info_v1_header::compression_type::bitfields:
					case bitmap_info_v1_header::compression_type::alpha_bitfields: { // wikipedia suggests that alpha_bitfields is valid on Windows CE only
						// TODO: error handling, throw. Header is inconsistent
						break;
					}
					case bitmap_info_v1_header::compression_type::rgb: {
						channel_num = 3;
						break;
					}
					case bitmap_info_v1_header::compression_type::cmyk: {
						channel_num = 4;
						break;
					}
					default: {
						// TODO: error handling, invariant violation; compression is used.
					}
				}

				size_t bits_per_channel = pixel_bit_size / channel_num;
				uint32_t channel_mask = (1 << bits_per_channel) - 1;

				for (ptrdiff_t i = 0; i < channel_num; ++i) {
					result[i] = channel_mask << (channel_num - 1 - i);
				}
				result[channel_num] = 0;

				// size_t channels_shift = bits_per_channel * channel_num;
				// size_t X_mask_shift = pixel_bit_size - channels_shift;
				// result.back() = ((1 << X_mask_shift) - 1) << channels_shift;
			}

			std::span significant_masks = std::span(result).first<std::tuple_size_v<result_t> - 1>();
			// move non-zero masks to the begining
			std::stable_partition(significant_masks.begin(), significant_masks.end(), 
				[](uint32_t item) -> bool { return item != 0; });

			uint32_t channels_mask = std::reduce(significant_masks.begin(), significant_masks.end(), (uint32_t)(0), 
				[](uint32_t lhs, uint32_t rhs) -> uint32_t { return lhs | rhs; });
			uint32_t pixel_mask = ((uint64_t)(1) << pixel_bit_size) - 1;

			result.back() = channels_mask ^ pixel_mask;

			// validate X mask
			bool valid = true;

			if (result.back() != 0) {
				uint32_t X_mask = result.back() >> std::countr_zero(result.back());
				valid &= ((X_mask + 1) == std::bit_ceil(X_mask));
			}

			if (!valid) {
				// TODO: handle error
			}

			return result;
		}
	};

	return std::visit(visitor, std::get<dib_header_index>(this->headers).first);
}

size_t bmp_header_protocol::get_pixel_bit_size() const {
	return std::visit([](const auto& h) -> size_t {
			return h.get_BitsPerPixel();
		},
		std::get<dib_header_index>(this->headers).first);
}

ptrdiff_t bmp_header_protocol::get_image_data_offset() const {
	return std::get<header_record_t<bitmap_file_header>>(this->headers).first.get_ImageOffset();
}

size_t bmp_header_protocol::header_size() const {
	return bitmap_file_header::size() + this->dib_header_size();
}

size_t bmp_header_protocol::file_size() const {
	return std::get<header_record_t<bitmap_file_header>>(this->headers).first.get_Size();
}

std::span<std::byte> bmp_header_protocol::commit(std::span<std::byte> dst) {
	bool valid = true;
	valid &= (dst.size() >= this->header_size());
	if (!valid) {
		// TODO: error handling
	}

	auto current_header_area = dst;

	auto write_header = [&]<typename hT>(bool if_present) -> void {
		if (if_present) {
			valid &= (current_header_area.size() >= hT::size());
			if (!valid) {
				// TODO: error handling
			}

			auto& [header, flag] = std::get<header_record_t<hT>>(this->headers);
			const auto& content = header.commit();
			std::copy_n(content.cbegin(), content.size(), current_header_area.begin());
			flag = true;

			current_header_area = current_header_area.subspan<hT::size()>();
		}
	};

	write_header.template operator()<bitmap_file_header>(true);
	std::visit([&](auto& h) -> void {
			using header_t = std::remove_cvref_t<decltype(h)>;
				
			valid &= (current_header_area.size() >= header_t::size());
			if (!valid) {
				// TODO: error handling
			}

			const auto& content = h.commit();
			std::copy_n(content.cbegin(), content.size(), current_header_area.begin());
			std::get<dib_header_index>(this->headers).second = true;

			current_header_area = current_header_area.subspan<header_t::size()>();
		},
		std::get<dib_header_index>(this->headers).first);

	write_header.template operator()<bitmap_info_v1_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v1));
	write_header.template operator()<bitmap_info_v2_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v2));
	write_header.template operator()<bitmap_info_v3_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v3));
	write_header.template operator()<bitmap_info_v4_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v4));
	write_header.template operator()<bitmap_info_v5_header>(this->if_contains_dib_header(dib_header_type::bitmap_info_v5));

	return dst.first(dst.size() - current_header_area.size());
}

size_t bmp_header_protocol::get_image_depth() const {
	return std::visit(overloaded_callable{
			[](const bitmap_core_v1_header& h) -> size_t {
				return 1;
			}, 
			[this](const bitmap_core_v2_header& h) -> size_t {
				auto channel_masks = this->get_channel_masks();

				auto significant_masks = std::span(std::as_const(channel_masks)).first<channel_num>();
				return std::accumulate(significant_masks.begin(), significant_masks.end(), (uint32_t)(0),
					[](uint32_t acc, uint32_t item) -> uint32_t {
						return acc + (item > 0);
					});
			}
		}, 
		std::get<dib_header_index>(this->headers).first);
}

size_t bmp_header_protocol::dib_header_size() const {
	return std::visit([](const auto& h) -> size_t { return h.get_HeaderSize(); },
		std::get<dib_header_index>(this->headers).first);
}

size_t bmp_header_protocol::hint_header_size(std::span<const std::byte, header_preamble_size> raw_data) {
	constexpr size_t sample_header_size = bitmap_file_header::size() + bitmap_core_v2_header::size();

	std::array<std::byte, sample_header_size> header_buffer{};

	// dump default core_v2 header, then override dib size field, then try read overriden 
	// header; if error occurs during read, then suggest core_v1 size
	std::span file_header_view = std::span(header_buffer).first<bitmap_file_header::size()>();
	std::span core_header_view = std::span(header_buffer).subspan<bitmap_file_header::size()>();

	static_assert(decltype(core_header_view)::extent == bitmap_core_v2_header::size());
	std::copy_n(bitmap_core_v2_header().commit().cbegin(), core_header_view.size(), core_header_view.begin());

	std::copy_n(raw_data.begin(), raw_data.size(), header_buffer.begin());

	// make sure the header is valid so far and file header can be created in place:
	bitmap_file_header dummy(file_header_view);

	try {
		return bitmap_file_header::size() + bitmap_core_v2_header(core_header_view).get_HeaderSize();
	} catch (const cli::bmp::header_invalid& e) {
		return bitmap_file_header::size() + bitmap_core_v1_header::size();
	}
}
