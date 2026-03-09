#include "parameters.hpp"

#include "compress/validation.hpp"
#include "restore/validation.hpp"
#include "commands/compress.hpp"
#include "commands/restore.hpp"

namespace cli::parameters {

namespace {

	template <typename T>
	struct validator_mapping {
		using validator_t = validate::validation_context(const typename T::value_t&);

		T command_line_parameters::* arg_ptr;
		validator_t* validator_ptr;
	};

	template <typename T>
	struct execution_mapping {
		using executor_t = void(const typename T::value_t&);

		T command_line_parameters::* arg_ptr;
		executor_t* validator_ptr;
	};


	constexpr std::tuple validation_mappings = {
		validator_mapping{ &command_line_parameters::compress_cx, validate::compress::validate_parameters }, 
		validator_mapping{ &command_line_parameters::restore_cx, validate::restore::validate_parameters }
	};

	constexpr std::tuple execution_mappings = {
		execution_mapping{ &command_line_parameters::compress_cx, command::compress::compress_command_handler }, 
		execution_mapping{ &command_line_parameters::restore_cx, command::restore::restore_command_handler }
	};

}

bool command_line_parameters::if_empty() const {
	auto has_value_acc = [](const auto&... optionals) -> bool {
		return ((optionals.context.has_value()) | ...);
	};
	return !has_value_acc(compress_cx, restore_cx);
}

validate::validation_context command_line_parameters::validate() const {
	validate::validation_context result{ u8"command"sv };

	auto validate_members = [&]<size_t... indices>(std::index_sequence<indices...>) -> void {
		auto item_handler = [&]<size_t index>() -> void {
			if ((this->*(std::get<index>(validation_mappings).arg_ptr)).context) {
				result.nest_context(
					std::invoke(std::get<index>(validation_mappings).validator_ptr,
						(this->*(std::get<index>(validation_mappings).arg_ptr)).context.value()));
			}
		};

		((item_handler.template operator()<indices>()), ...);
	};

	validate_members(std::make_index_sequence<std::tuple_size_v<decltype(validation_mappings)>>());

	return result;
}


void command_line_parameters::execute() const {
	auto execute_members = [&]<size_t... indices>(std::index_sequence<indices...>) -> void {
		auto item_handler = [&]<size_t index>() -> void {
			if ((this->*(std::get<index>(execution_mappings).arg_ptr)).context) {
				std::invoke(std::get<index>(execution_mappings).validator_ptr,
					(this->*(std::get<index>(execution_mappings).arg_ptr)).context.value());
			}
		};

		((item_handler.template operator()<indices>()), ...);
	};

	execute_members(std::make_index_sequence<std::tuple_size_v<decltype(execution_mappings)>>());
}

}
