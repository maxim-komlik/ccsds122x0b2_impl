#pragma once

#include <tuple>
#include <array>
#include <memory>
#include <algorithm>
#include <functional>
#include <cstddef>

#include "common/utils.hpp"

#include "parameters_context.hpp"
#include "help_context.hpp"
#include "expected.hpp"
#include "utility.hpp"
#include "exception/exception.hpp"

template <typename Param>
class contextual_parser {
private:
	using target_traits = parameter_context<Param>;

	using immediates_t = decltype(target_traits::immediates);
	using named_t = decltype(target_traits::named);

	static constexpr size_t immediates_size = std::tuple_size_v<immediates_t>;
	static constexpr size_t named_size = std::tuple_size_v<named_t>;

	static constexpr size_t unknown_param_index = std::numeric_limits<size_t>::max();

private:
	template <typename T>
	struct parse_named;

	template <>
	struct parse_named <std::tuple<>> {
	private:
		using subject_t = std::tuple<>;
	public:
		using type = subject_t;
	};

	template <typename... Ts>
	struct parse_named<std::tuple<Ts...>> {
	private:
		using subject_t = std::tuple<Ts...>;
		using next_t = tuple_remove_first_element_t<subject_t>;

	public:
		using type = tuple_prepend_element_t<
			typename parse_named<next_t>::type,
			typename tuple_element_first_t<subject_t>::value_t>;
	};

	template <typename T>
	using parse_named_t = parse_named<std::remove_const_t<T>>::type;

	template <typename T>
	using parse_immediates_t = parse_named<std::remove_const_t<T>>::type;


	using storage_type = tuple_merge_t<
		parse_immediates_t<immediates_t>,
		parse_named_t<named_t>>;

	static constexpr size_t storage_size = std::tuple_size_v<storage_type>;

private:
	storage_type storage;
	std::array<bool, storage_size> processed = { false };

public:
	template <size_t index>
	const auto& get() const {
		return std::get<index>(this->storage);
	}

	template <size_t index>
	bool if_default() const {
		// either bool has valid value, either parsing failed and exception was thrown
		return !this->processed[index];
	}

	consteval static size_t name_to_index(std::string_view target_name) {
		static_assert(named_size > 0, "No named parameters defined");
		return immediates_size + contextual_parser::name_to_index_impl<named_size - 1>(target_name);
	}

	void parse(std::vector<std::string_view>& tokens) {
		try {
			parse_impl(tokens);
		} catch (const cli::context_exit_requested& e) {
			tokens.pop_back();
		}

		if (std::any_of(this->processed.cbegin(), this->processed.cend(),
				[](bool param) -> bool { return param == false; })) {
			std::invoke(
				[&]<size_t... indices>(std::index_sequence<indices...>) {
					auto item_handler = [&]<size_t index>() {
						if constexpr (index >= immediates_size) {
							if (this->processed[index] == false) {
								constexpr size_t named_index = index - immediates_size;
								const auto& default_optional = named_description<named_index>().default_value;
								if (!default_optional) {
									// option is mandatory and is not provided. 
									throw cli::parameter_mandatory(named_description<named_index>().name);
								}
								std::get<index>(this->storage) = default_optional.value();
							}
						}
					};

					((item_handler.template operator()<indices>()), ...);
				}, 
				std::make_index_sequence<storage_size>());
		}
	}

	template <size_t index>
	static consteval auto get_default() {
		// static_assert(index >= immediates_size);
		// return named_description<index - immediates_size>().default_value.value();
		return get_description<index>().default_value.value();
	}

private:
	void parse_impl(std::vector<std::string_view>& tokens) {
		auto handle_parsing_error = [&]<size_t index>(const cli::unexpected& reason) {
			const auto& description = get_description<index>();
			switch (reason.ec) {
			case std::errc::invalid_argument: {
				try_parse_special_tokens<index>(tokens);

				const auto& default_optional = description.default_value;
				if (!default_optional) {
					// or description.name?
					throw cli::parameter_mandatory(make_parameter_name<index>());
				} else {
					if (index >= immediates_size) {
						// for named parameter, if key is provided, then the value is necessary
						std::string invalid_value = static_cast<std::string>(tokens.back());
						throw cli::parameter_invalid(description.name, std::move(invalid_value), reason.reason);
					} else {
						std::get<index>(this->storage) = default_optional.value();
						// considered as expected flow for non-mandatory immediate params
					}
				}
				break;
			}
			case std::errc::protocol_error: {
				std::string invalid_value = static_cast<std::string>(tokens.back());
				throw cli::parameter_invalid(make_parameter_name<index>(), std::move(invalid_value), reason.reason);
				break;
			}
			default: {
				// TODO: should be unreachable?
			}
			}
		};

		// immediates parsing
		auto immediates_parser = [&]<size_t... indices>(std::index_sequence<indices...>) {
			auto param_parser = [&]<size_t index>() {
				using description_t = std::tuple_element_t<index, immediates_t>;
				cli::expected<typename description_t::value_t> parse_result = description_t::parser_t().parse(tokens);
				if (!parse_result) {
					handle_parsing_error.template operator()<index>(parse_result.error());
					return;
				}

				std::get<index>(this->storage) = parse_result.value();
				this->processed[index] = true;	// to track where defaults assigned
			};

			((param_parser.template operator()<indices>()), ...);
		};
		immediates_parser(std::make_index_sequence<immediates_size>());

		// named parameters parsing
		if (named_size > 0) {
			bool key_matched = false;
			do {
				const std::string_view key = tokens.back();
				key_matched = false;

				std::invoke(
					[&]<size_t... indices>(std::index_sequence<indices...>) {
						auto item_handler = [&]<size_t index>() {
							if (std::get<index>(target_traits::named).name == key) {
								constexpr size_t storage_index = index + immediates_size;
								key_matched = true;
								tokens.pop_back();

								using description_t = std::tuple_element_t<index, named_t>;
								cli::expected<typename description_t::value_t> parse_result = description_t::parser_t().parse(tokens);
								if (!parse_result) {
									// e.g. integer is parsed, but does not satisfy constraints/min-max limits
									handle_parsing_error.template operator()<storage_index>(parse_result.error());
								}

								std::get<storage_index>(this->storage) = parse_result.value();
								this->processed[storage_index] = true;
							}
						};

						((item_handler.template operator()<indices>()), ...);
					}, 
					std::make_index_sequence<named_size>());
			} while (key_matched);

			try_parse_special_tokens<unknown_param_index>(tokens);
		}
	}

	template <size_t index>
	bool try_parse_special_tokens(std::vector<std::string_view>& tokens) {
		std::string_view token = tokens.back();
		bool handled = false;

		// -- : 
		//		throw exception and catch it on exit of the current scope. Check mandatory parameters
		// --help :
		//		--scope param1 param2 --key1 kparam1 --pos1 ppos1 --pflag1
		//			either context help, either param help.
		//			if value is expected, show param help
		//			after value/key-value is parsed, show context help
		//		kind of special case: on index 0 show context help and param help for the first parameter
		//		we have no idea where params for current scope end, may be several same named or 
		//		variable length sequence.
		// 
		// param help:
		//	+ name
		//	+ value placeholder
		//	+ param description
		//	+ parser requirement
		// 
		// immediate parameters miss param description?
		// but should have it. therefore name section should be optional?
		// e.g. --frame 1024 512 1
		// or --frame description should mention value order?
		// 
		// --shifts is example where parent name description becomes messy
		// 
		// context help:
		//	+ sequence of immediate placeholders
		//	+ named placeholder, if has named parameters
		//	+ variable length sequence placeholder, if has vla
		//	+ list of named parameters, with corresponding param description; 
		//		if mandatory specification, and default value description if optional
		//	+ vla description section, if applicable
		//	
		// --help is meaningful in some nested context. After --help token is parsed, 
		// parsing stops, unparsed tokens are skipped.
		// build help description, allocate dynamically, pass to exception object and throw.
		// handle user interface logic in the root frame.
		// 
		// differentiate between context help and param help? 
		//	vector of strings divided by paragraphs?
		//	variant with strong types?
		//	base pointer with derived implementations? but interfaces are not the same
		//	
		// context help and param help are not exclusive! both needed when --help token is parsed 
		// at position 0 parameter
		//

		using namespace cli::parameters;

		if (token == global_context::help.name) {
			handled = true;
			auto help_cx = std::make_unique<help_context>();
			if (index < storage_size) {
				constexpr size_t storage_index = (index == unknown_param_index) ? 0 : index;
				help_cx->param_help = make_parameter_help<storage_index>();
			}

			if ((index == 0) | (index == unknown_param_index)) {
				help_cx->cx_help = make_context_help();
			}

			if (index == 0) {
				help_cx->global_cx_help = help_context::make_global_help();
			}

			throw cli::help_requested(std::move(help_cx));
		} else if (token == global_context::context_exit.name) {
			handled = true;
			throw cli::context_exit_requested();
		}

		return handled;
	}

	template <size_t index>
	constexpr static auto get_description() {
		if constexpr (index >= immediates_size) {
			return named_description<index - immediates_size>();
		}
		else {
			return immdediate_description<index>();
		}
	}


	template <size_t index, typename T>
	struct default_value_if_structural {
		using type = default_value_if_structural;

	private:
		using parser_t = decltype(contextual_parser::get_description<index>())::parser_t;

	public:
		static constexpr std::string_view value = meta::as_string<
			static_cast<parser_t::default_representation_t>(
				contextual_parser::get_description<index>().default_value.value())>::parse();
	};

	template <size_t index, typename T>
	using default_value_if_structural_t = default_value_if_structural<index, T>::type;


	template <size_t index, typename T, bool if_enum>
	struct default_value_if_enum;

	template <size_t index, typename T>
	struct default_value_if_enum<index, T, false> {
		using type = default_value_if_structural_t<index, T>;
	};

	template <size_t index, typename T>
	struct default_value_if_enum<index, T, true> {
		using type = default_value_if_enum;

	private:
		using parser_t = decltype(contextual_parser::get_description<index>())::parser_t;

	public:
		static constexpr std::string_view value = parser_t::template get_enumerator_string<
			contextual_parser::get_description<index>().default_value.value()>();
	};

	template <size_t index, typename T>
	using default_value_if_enum_t = default_value_if_enum<index, T, std::is_enum_v<T>>::type;


	template <size_t index, typename T, bool if_integral>
	struct default_value_if_integral;

	template <size_t index, typename T>
	struct default_value_if_integral<index, T, false> {
		using type = default_value_if_enum_t<index, T>;
	};

	template <size_t index, typename T>
	struct default_value_if_integral<index, T, true> {
		using type = default_value_if_integral;

		static constexpr std::string_view value = meta::materialize<
			meta::to_static_string<
				std::integral_constant<T, 
					contextual_parser::get_description<index>().default_value.value()>>()>();
	};

	template <size_t index, typename T>
	using default_value_if_integral_t = default_value_if_integral<index, T, std::is_integral_v<T>>::type;


	template <size_t index, typename T, bool if_integral>
	struct default_value_if_bool;

	template <size_t index, typename T>
	struct default_value_if_bool<index, T, false> {
		using type = default_value_if_integral_t<index, T>;
	};

	template <size_t index, typename T>
	struct default_value_if_bool<index, T, true> {
		using type = default_value_if_bool;

	public:
		static constexpr std::string_view value = std::invoke([]() constexpr {
				using namespace std::literals;

				constexpr bool default_value = contextual_parser::get_description<index>().default_value.value();
				if (default_value) {
					return "true"sv;
				}
				else {
					return "false"sv;
				}
			});
	};

	template <size_t index, typename T>
	using default_value_if_bool_t = default_value_if_bool<index, T, std::is_same_v<T, bool>>::type;


	template <size_t index, bool has_default>
	struct get_default_if_present;

	template <size_t index>
	struct get_default_if_present<index, false> {
		static constexpr std::string_view value = {};
	};

	template <size_t index>
	struct get_default_if_present<index, true> {
		using value_t = decltype(contextual_parser::get_description<index>())::value_t;

		static constexpr std::string_view value = default_value_if_bool_t<index, value_t>::value;
	};


	template <size_t index>
	static consteval std::string_view make_default_value_description() {
		// for bools - map to strings for true and false
		// for integers - meta parse as template parameter
		// for enums - mapping to enum trait
		// for structural - meta parse via template value parameter, using name fields trait
		// 
		// pass index as template parameter for high level handler. pass default.has_value() as 
		// template parameter as well, specialize for explicit true value.
		// 

		constexpr auto description = get_description<index>();
		return get_default_if_present<index, description.default_value.has_value()>::value;
	};

	template <size_t index>
	static consteval std::string_view make_parameter_name() {
		if (index >= immediates_size) {
			return get_description<index>().name;
		}

		constexpr auto open = meta::make_static_string(meta::trim_terminator(std::span{ "<" }));
		constexpr auto close = meta::make_static_string(meta::trim_terminator(std::span{ ">" }));
		constexpr auto number = meta::to_static_string<std::integral_constant<size_t, index + 1>>();

		return meta::materialize<open + number + close>();
	}


	template <size_t index>
	static constexpr help_context::description make_parameter_help() {
		constexpr auto description = get_description<index>();
		return help_context::description{
			index < immediates_size, 
			!description.default_value.has_value(), 
			false, 
			make_parameter_name<index>(),
			description.help_description, 
			decltype(description)::parser_t::placeholder, 
			make_default_value_description<index>(),
			decltype(description)::parser_t::requirements
		};
	}

	static help_context::context_help make_context_help() {
		std::vector<help_context::description> option_descriptions;
		option_descriptions.reserve(immediates_size + named_size);

		std::invoke(
			[&option_descriptions]<size_t... indices>(std::index_sequence<indices...>) {
				(option_descriptions.push_back(make_parameter_help<indices>()), ...);
			}, 
			std::make_index_sequence<immediates_size + named_size>{});

		auto named_begin = std::stable_partition(option_descriptions.begin(), option_descriptions.end(),
			[](const help_context::description& item) {
				return item.if_positional;
			});

		auto named_optional_begin = std::partition(named_begin, option_descriptions.end(),
			[](const help_context::description& item) {
				return item.if_mandatory;
			});

		// but is redundant per class requirements
		auto immediate_optional_begin = std::stable_partition(option_descriptions.begin(), named_begin,
			[](const help_context::description& item) {
				return item.if_mandatory;
			});

		using namespace std::literals;
		constexpr std::string_view space = " "sv;
		constexpr std::string_view optional_open = "["sv;
		constexpr std::string_view optional_close = "]"sv;

		using const_iterator_t = decltype(option_descriptions)::const_iterator;

		// explicit cast to const iterator to avoid ambiguous type deduction context in algorithm calls below
		auto as_const_iter = [](auto iter) -> const_iterator_t {
			return iter;
		};

		std::string overview;

		std::for_each(option_descriptions.cbegin(), as_const_iter(immediate_optional_begin),
			[&overview](const auto& item) {
				overview += item.placeholder;
				overview += space;
			});

		if (immediate_optional_begin != named_begin) {
			overview += optional_open;

			std::for_each(immediate_optional_begin, named_begin,	// as_const_iter?
				[&overview](const auto& item) {
					overview += item.placeholder;
					overview += space;
				});

			overview += optional_close;
		}

		std::for_each(named_begin, named_optional_begin,	// as_const_iter?
			[&overview](const auto& item) {
				overview += item.name;
				overview += space;
				overview += item.placeholder;
				overview += space;
			});

		if (named_optional_begin != option_descriptions.cend()) {
			overview += optional_open;

			std::for_each(as_const_iter(named_optional_begin), option_descriptions.cend(),
				[&overview](const auto& item) {
					overview += item.name;
					overview += space;
					overview += item.placeholder;
					overview += space;
				});

			overview += optional_close;
		}

		return help_context::context_help{ overview, option_descriptions };
	}

private:
	template <size_t index>
	constexpr static std::tuple_element_t<index, named_t> named_description() {
		return std::get<index>(target_traits::named);
	}

	template <size_t index>
	constexpr static std::tuple_element_t<index, immediates_t> immdediate_description() {
		return std::get<index>(target_traits::immediates);
	}

	template <size_t index>
	consteval static size_t name_to_index_impl(std::string_view target) {
		if (contextual_parser::named_description<index>().name == target) {
			return index;
		}

		if constexpr (index == 0) {
			throw std::out_of_range("Parameter name was not defined");
		}
		else {
			return contextual_parser::name_to_index_impl<index - 1>(target);
		}
	}
};
