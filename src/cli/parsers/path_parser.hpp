#pragma once

#include <string_view>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iterator>
#include <cstddef>

#include "cli.hpp"
#include "expected.hpp"
#include "utility.hpp"

namespace cli::parsers {

template <bool allow_non_existing = true, bool allow_glob = false>
struct path_parser {
	using value_t = std::filesystem::path;

	cli::expected<value_t> parse(std::vector<std::string_view>& tokens) {
		value_t result;
		
		try {
			result = std::filesystem::path(tokens.back());
		} catch (const std::filesystem::filesystem_error& e) {
			// here dynamically allocated strings come...
			return cli::unexpected{ std::errc::invalid_argument, invalid_parameter_description };
		}

		result = std::filesystem::weakly_canonical(result);
		auto exist_check_target = result;
		
		if (if_pattern(result)) {
			if (!allow_glob) {
				return cli::unexpected{ std::errc::protocol_error};
			}

			auto expanded_sample = find_matching_path(result);
			if (expanded_sample) {
				exist_check_target = std::move(expanded_sample).value();
			} else {
				return cli::unexpected{ std::errc::protocol_error, expanded_sample.error().reason };
			}
		}

		if (!allow_non_existing) {
			if (!std::filesystem::exists(exist_check_target)) {
				return cli::unexpected{ std::errc::protocol_error };
			}
		}

		tokens.pop_back();
		return result;
	}

private:
	static consteval std::string_view generate_requirements() {
		constexpr auto general = meta::make_static_string(meta::trim_terminator(
			std::span{ "Parameter value must represent valid filesystem path" }));
		constexpr auto dot = meta::make_static_string(meta::trim_terminator(std::span{ ". " }));

		constexpr auto constraint_exists = meta::make_static_string(meta::trim_terminator(
			std::span{ " and must refer to accessible filesystem object" }));
		constexpr auto constraint_pattern = meta::make_static_string(meta::trim_terminator(
			std::span{ "Value may contain pattern expansion tokens * and **. " }));

		constexpr size_t size_p1 = general.size() + (constraint_exists.size() & 
			(-(std::make_signed_t<size_t>)(!allow_non_existing)));
		constexpr size_t size_p2 = constraint_pattern.size() & (-(std::make_signed_t<size_t>)(allow_glob));

		return meta::materialize<(general + constraint_exists).first<size_p1>() + dot + 
			constraint_pattern.first<size_p2>()>();
	}

	static constexpr std::string_view invalid_parameter_description = "Couldn't parse path parameter. "sv;
	static constexpr std::string_view root_is_pattern_description = "Path root must not contain expansion pattern. "sv;
	static constexpr std::string_view root_not_found_description = "Path must have accessible root. "sv;
	static constexpr std::string_view invalid_substitution_description = "All pattern expansions refer to unaccessible filesystem entities. "sv;
	static constexpr std::string_view pattern_expansion_failed_description = "Couldn't expand path pattern due to unexpected error. "sv;
	
	static constexpr std::u8string_view glob_token = u8"*"sv;
	static constexpr std::u8string_view multiglob_token = u8"**"sv;

	static bool if_pattern(const std::filesystem::path& path) {
		std::u8string str = path.u8string();
		std::u8string_view str_view = str;
		return str_view.find(glob_token) != decltype(str_view)::npos;
		// return std::search(str.cbegin(), str.cend(), glob_token.cbegin(), glob_token.cend()) != str.cend();
	}

	static cli::expected<std::filesystem::path> find_matching_path(const std::filesystem::path& pattern) {
		// TODO: should be noexcept and return all thrown exception descriptions as unexpected?

		// cases:
		//	single * as part of path token:
		//		i.e. token has form "prefix*postfix"
		//		iterate over items in parent dir. check if prefix matches on current item; if so:
		//			check if postfix matches; if so — head extension is found 
		//				if last path token, then it is suitable result?
		//				otherwise, 
		//					add sample token to the head pattern
		//					continue expanding subseqent path tokens
		//					if matching branch is found, then it is suitable result?
		//			otherwise, continue with next dir item
		// 
		//	multiple * as part of path token:
		//		i.e. token has form "prefix0*prefix1*prefix2*...*prefixN*postfix"
		//		treat case as single * case, trying to match consequtive prefixes gradually:
		//			for every prefix match found, set current matched prefix size
		//			check if target path token tail contains next prefix
		//			if all prefixes matched, try to match postfix, continue as single * case.
		//	
		// path token containing glob token must correspond to existing filesystem entity
		//		i.e. if glob substitution failed, the pattern is invalid, parsing error should be reported
		// and otherwise regular existanse check applied: if after glob token substitution no sample is found, 
		// then referred filesystem entity does not exist
		// 
		// multiglob is valid as whole path token only.
		//


		// path must be canonical and must be pattern (which implies non-empty)
		if (if_pattern(pattern.root_path())) {
			// parsing error: root is glob
			return cli::unexpected{ std::errc::invalid_argument, root_is_pattern_description };
		}
		if (!std::filesystem::exists(pattern.root_path())) {
			// parsing error: root does not exist
			return cli::unexpected{ std::errc::invalid_argument, root_not_found_description };
		}

		std::vector<std::filesystem::path> heads;
		std::vector<std::filesystem::directory_iterator> dir_iters;

		{
			// good approximation for non-multiglob patterns
			size_t pattern_depth = std::distance(pattern.begin(), pattern.end());
			heads.reserve(pattern_depth);
			dir_iters.reserve(pattern_depth);
		}
		
		heads.push_back(pattern.root_path());

		std::filesystem::path result{};
		size_t known_glob_depth = 1;
		size_t valid_glob_depth = 0;

		auto current_token = std::next(pattern.begin());

		do {
			bool new_entry = false;
			for (; current_token != pattern.end(); ++current_token) {
				if (if_pattern(*current_token)) {
					// TODO: handle multiglob here?
					break;
				}

				heads.back() /= *current_token;
				new_entry = true;
			}

			if (current_token != pattern.end()) {
				// must be token containing glob token
				if (!std::filesystem::exists(heads.back())) {
					// glob must substitute into existing path, hence parent path must exist
					known_glob_depth = std::max(known_glob_depth, heads.size());

					// dummy item to skip substitution attempt
					dir_iters.push_back(std::filesystem::directory_iterator{});


					// auto current_depth = std::distance(heads.back().begin(), heads.back().end());
					// heads.pop_back();
					// auto reenter_branch_depth = std::distance(heads.back().begin(), heads.back().end());
					// std::advance(current_token, reenter_branch_depth - current_depth);
					// 
					// if (dir_iters.empty()) {
					// 	// TODO: parsing error: most outer glob does not correspond to existing entity
					// }
				} else {
					valid_glob_depth = std::max(valid_glob_depth, heads.size());
					if (new_entry) {
						// access-denied-like errors are to be handled properly by processing implementaion, 
						// no reasonable handling is possible due to lack of usage scenarios knowledge
						dir_iters.push_back(std::filesystem::directory_iterator(
							heads.back(), std::filesystem::directory_options::skip_permission_denied));
					}
				}

				const std::u8string glob_str = current_token->u8string();
				std::u8string_view glob_view = glob_str;
				bool pattern_matched = false;

				while (dir_iters.back() != std::filesystem::directory_iterator{}) {
					const std::u8string item_str = dir_iters.back()->path().filename().u8string();
					std::u8string_view item_view = item_str;

					{
						size_t glob_prefix_begin_pos = 0;
						size_t item_matched_prefix_pos = 0;
						std::u8string_view current_prefix;
						do {
							current_prefix = glob_view.substr(glob_prefix_begin_pos,
								glob_view.find(glob_token, glob_prefix_begin_pos) - glob_prefix_begin_pos);
							auto prefix_pos = item_view.find(current_prefix, item_matched_prefix_pos);

							if (prefix_pos == decltype(item_view)::npos) {
								break;
							}

							glob_prefix_begin_pos += current_prefix.size() + glob_token.size();
							item_matched_prefix_pos = prefix_pos + current_prefix.size();
						} while (glob_prefix_begin_pos < glob_view.size());

						if (!(glob_prefix_begin_pos < glob_view.size())) {
							// greedy matching doesn't work well for postfix. Assuming the token right 
							// before postfix is *, it can match everything between the last matched 
							// prefix and the postfix, therefore we can try exact match for postfix
							auto postfix = current_prefix;
							if (glob_prefix_begin_pos == glob_view.size()) {
								// this means the postfix is empty, because otherwise it would be same 
								// as last prefix, but pos would be greater by 1 due to size of * token
								postfix = u8""sv;
							}

							size_t postfix_pos = item_view.size() - postfix.size();
							if (item_view.substr(postfix_pos) == postfix) {
								item_matched_prefix_pos = postfix_pos + postfix.size();
							}
						}

						pattern_matched = !(glob_prefix_begin_pos < glob_view.size()) &
							(item_matched_prefix_pos == item_view.size());
					}

					if (pattern_matched) {
						heads.push_back(dir_iters.back()->path());
						++(dir_iters.back());
						++current_token;
						break;
					}

					++(dir_iters.back());
				}

				if (!pattern_matched) {
					auto current_depth = std::distance(heads.back().begin(), heads.back().end());
					dir_iters.pop_back(); 
					heads.pop_back();

					auto reenter_branch_depth = std::distance(heads.back().begin(), heads.back().end());

					std::advance(current_token, reenter_branch_depth - current_depth);
				}
			} 
		
			if (current_token == pattern.end()) {
				result = heads.back();
				break;
			}
		} while (!heads.empty());
		
		if (known_glob_depth > valid_glob_depth) {
			// all glob substitutions are invalid
			return cli::unexpected{ std::errc::invalid_argument, invalid_substitution_description };
		}

		return result;
	}

public:
	static constexpr std::string_view requirements = generate_requirements();
	static constexpr std::string_view placeholder = "<path>"sv;
};

}
