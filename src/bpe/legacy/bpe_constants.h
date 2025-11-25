#pragma once

#include <array>
#include <bit>

// TODO: move compile time parameters to dedicated files
namespace dbg {
	namespace bpe {
		static constexpr uint32_t mask_quant_DC = 0x01;
		static constexpr uint32_t mask_additional_DC = 0x02;
		static constexpr uint32_t mask_bdepth_AC = 0x04;
		static constexpr uint32_t mask_stage_0 = 0x08;
		static constexpr uint32_t mask_stage_1 = 0x010;
		static constexpr uint32_t mask_stage_2 = 0x020;
		static constexpr uint32_t mask_stage_3 = 0x040;
		static constexpr uint32_t mask_stage_4 = 0x080;

		static constexpr uint32_t mask_validation = 0x01000;

		namespace encoder {
			static constexpr uint32_t disabled_stages = 0x00;

			constexpr bool if_enabled(uint32_t mask) {
				return (disabled_stages & mask) == 0;
			}

			constexpr bool if_disabled(uint32_t mask) {
				return !if_enabled(mask);
			}
		};

		namespace decoder {
			static constexpr uint32_t disabled_stages = 0x00;

			constexpr bool if_enabled(uint32_t mask) {
				return (disabled_stages & mask) == 0;
			}

			constexpr bool if_disabled(uint32_t mask) {
				return !if_enabled(mask);
			}
		};
	};
};

// used to inject constants and implementation detail internal structures
class bpe_constants_helper {
protected:

	template <typename T>
	struct kParams {
		T* data;
		size_t datalength;
		ptrdiff_t bdepth;
		T reference;
	};
	
	static constexpr size_t max_subband_shift = 3;

	static constexpr size_t items_per_block = 64;	// TODO: global constant as a property of a block struct
	static constexpr size_t block_size_mask = items_per_block - 1;
	static constexpr size_t block_size_shift = std::bit_width(std::bit_ceil(items_per_block) - 1); // approx for log

	static constexpr size_t items_per_gaggle = 16; // the value is assumed to be POT, see below
	static constexpr size_t gaggle_size_mask = items_per_gaggle - 1;
	static constexpr size_t gaggle_size_shift = std::bit_width(std::bit_ceil(items_per_gaggle) - 1); // approx for log

	// entropy
	static constexpr size_t min_vlw_length = 0;
	static constexpr size_t max_vlw_length = 4;
	static constexpr size_t vlw_lengths_count = 5;

	static constexpr size_t min_coded_vlw_length = 2;
	static constexpr size_t max_coded_vlw_length = max_vlw_length;
	static constexpr size_t coded_vlw_lengths_count = 3;

	static constexpr ptrdiff_t entropy_uncoded_codeoption = -1;

	static constexpr size_t min_N_value = 1;
	static constexpr size_t max_N_value = 10;
	static constexpr ptrdiff_t k_uncoded_codeoption = -1;

	static constexpr size_t symbol_types_H_codeoption = 0x02;
	static constexpr size_t symbol_tran_H_codeoption = 0x02;
	static constexpr size_t symbol_tran_D_codeoption = 0x01;

	static constexpr size_t F_num = 3;
	static constexpr size_t CpF_num = 1;
	static constexpr size_t HxpF_num = 4;
	static constexpr ptrdiff_t D_offset = 1;
	static constexpr ptrdiff_t C_offset = 2;
	static constexpr ptrdiff_t G_offset = 3;
	static constexpr ptrdiff_t Hx_offset = 4;
	static constexpr ptrdiff_t F_step = 8;
	static constexpr ptrdiff_t B_index = 0;
	static constexpr ptrdiff_t P_index = 8;
	static constexpr ptrdiff_t DC_index = 16;

	static constexpr std::array<uint64_t, 24> family_masks = std::invoke([]() constexpr {
		std::array<uint64_t, 24> result{ 0 };

		constexpr size_t gen_num = 2;
		constexpr ptrdiff_t C_gen_index = 0;
		constexpr ptrdiff_t G_gen_index = 1;

		// constexpr ptrdiff_t reserved_1_index = 8; // allocated to P_index
		// constexpr ptrdiff_t reserved_2_index = 16; // allocated to DC_index

		constexpr uint64_t X_mask = 0x0f;	// 4-bit mask for one group
		constexpr size_t X_mask_blen = 4;

		// for (ptrdiff_t i = (F_num - 1); i >= 0; --i) {
		for (ptrdiff_t i = 0; i < F_num; ++i) {
			// for (ptrdiff_t j = (gen_num - 1); j >= 0; --j) {
			for (ptrdiff_t j = 0; j < gen_num; ++j) {
				if (j == C_gen_index) {
					result[i * F_step + C_offset] =
						(X_mask << ((F_num - i - 1) * CpF_num * X_mask_blen))
							<< (F_num * HxpF_num * X_mask_blen);
				}
				if (j == G_gen_index) {
					// for (ptrdiff_t k = (HxpF_num - 1); k >= 0; --k) {
					for (ptrdiff_t k = 0; k < HxpF_num; ++k) {
						result[i * F_step + Hx_offset + k] =
							(X_mask << ((HxpF_num - k - 1) * X_mask_blen))
								<< ((F_num - i - 1) * HxpF_num * X_mask_blen);
						result[i * F_step + G_offset] |= result[i * F_step + Hx_offset + k];
					}
				}
				result[i * F_step + D_offset] |= result[i * F_step + C_offset + j];
			}
			result[B_index] |= result[i * F_step + D_offset];
		}
		result[P_index] = ((X_mask >> 1) << (F_num * (CpF_num + HxpF_num) * X_mask_blen));
		result[DC_index] = (uint64_t)(0x01) << ((sizeof(uint64_t) << 3) - 1);
		return result;
	});
	
	static constexpr std::array<uint64_t, 10> bitshift_masks = std::invoke([]() constexpr {
		std::array<uint64_t, 10> result = { 0 };

		constexpr size_t bshift_G_step = 3;
		constexpr size_t bshift_DC_index = 0;
		constexpr size_t bshift_P_offset = 1;
		constexpr size_t bshift_C_offset = bshift_P_offset + bshift_G_step;
		constexpr size_t bshift_G_offset = bshift_P_offset + 2 * bshift_G_step;

		for (size_t f = 0; f < F_num; ++f) {
			result[bshift_P_offset + f] = (uint64_t)(family_masks[DC_index]) >> (f + 1);
			result[bshift_C_offset + f] = family_masks[f * F_step + C_offset];
			result[bshift_G_offset + f] = family_masks[f * F_step + G_offset];
		}
		result[bshift_DC_index] = family_masks[DC_index];
		return result;
	});

};
