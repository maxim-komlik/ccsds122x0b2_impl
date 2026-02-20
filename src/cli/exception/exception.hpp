#pragma once

#include <string_view>
#include <string>
#include <memory>
#include <utility>

#include "help_context.hpp"

namespace cli {
	using namespace std::literals;

	class exception {
		static constexpr std::string_view description = "General command line interface application error. "sv;
	public:
		virtual ~exception() = default;
		exception() noexcept = default;
		exception(const exception& other) noexcept = default;
		exception& operator=(const exception& other) noexcept = default;

		// TODO: refactor return type, some kind of noexcept string_view
		virtual std::string_view what() const noexcept {
			return description;
		}
	};


	class parsing_error: public exception {
		static constexpr std::string_view description = "Unspecified error occured when parsing parameter. "sv;
		static constexpr std::string_view name_placeholder = "<unspecified>"sv;

	private:
		std::string_view name; // must refer to static const string

	public:
		parsing_error() noexcept = default;
		parsing_error(std::string_view parameter_name) : name(parameter_name) {}

		// TODO: refactor return type, some kind of noexcept string_view
		std::string_view what() const noexcept override {
			return description;
		}

		std::string_view parameter_name() const noexcept {
			if (this->name.empty()) {
				return name_placeholder;
			}
			return this->name;
		}
	};

	class parameter_mandatory: public parsing_error {
		static constexpr std::string_view description = "Value for mandatory parameter is not specified. "sv;

	public:
		virtual ~parameter_mandatory() = default;
		parameter_mandatory() noexcept = default;
		parameter_mandatory(const parameter_mandatory& other) noexcept = default;
		parameter_mandatory& operator=(const parameter_mandatory& other) noexcept = default;
		
		parameter_mandatory(std::string_view name) : parsing_error(name) {}

		// TODO: refactor return type, some kind of noexcept string_view
		std::string_view what() const noexcept override {
			return description;
		}
	};


	class parameter_invalid : public parsing_error {
		static constexpr std::string_view description = "Value for parameter is invalid. "sv;
		static constexpr std::string_view value_placeholder = "<unspecified>"sv;
		static constexpr std::string_view reason_placeholder = "<unspecified>"sv;

	private:
		std::string value;
		std::string_view violated_requirement;	// must refer to static const string

	public:
		~parameter_invalid() = default;
		parameter_invalid() noexcept = default;
		parameter_invalid(parameter_invalid&& other) noexcept = default;
		parameter_invalid& operator=(parameter_invalid&& other) noexcept = default;
		parameter_invalid(const parameter_invalid& other) = default;
		parameter_invalid& operator=(const parameter_invalid& other) = default;

		parameter_invalid(std::string_view name) noexcept : parsing_error(name) {}

		parameter_invalid(std::string_view name, std::string&& value, std::string_view reason) noexcept : 
			parsing_error(name), value(std::move(value)), violated_requirement(reason) {}

		// TODO: refactor return type, some kind of noexcept string_view
		std::string_view what() const noexcept override {
			return description;
		}

		std::string_view parameter_value() const noexcept {
			if (this->value.empty()) {
				return value_placeholder;
			}
			return this->value;
		}

		std::string_view reason() const noexcept {
			if (this->violated_requirement.empty()) {
				return reason_placeholder;
			}
			return this->violated_requirement;
		}
	};


	// helper wrapper to manage parameter lifetime correctly
	struct parameter_name_container {
		std::string name_storage;
	};


	class parameter_unknown: private parameter_name_container, public parsing_error {
		static constexpr std::string_view description = "Parameter is not expected. "sv;

	public:
		~parameter_unknown() = default;
		parameter_unknown() noexcept = default;
		parameter_unknown(parameter_unknown&& other) noexcept = default;
		parameter_unknown& operator=(parameter_unknown&& other) noexcept = default;
		parameter_unknown(const parameter_unknown& other) = default;
		parameter_unknown& operator=(const parameter_unknown& other) = default;

		parameter_unknown(std::string&& name) noexcept : 
			parameter_name_container(std::move(name)), parsing_error(name_storage) {}

		// TODO: refactor return type, some kind of noexcept string_view
		std::string_view what() const noexcept override {
			return description;
		}
	};


	class parsing_aborted : public exception {
		static constexpr std::string_view description = "Command line parsing stopped due to side effect execution. "sv;

	public:
		// TODO: refactor return type, some kind of noexcept string_view
		virtual std::string_view what() const noexcept {
			return description;
		}
	};

	class context_exit_requested : public parsing_aborted {
		static constexpr std::string_view description = "Current context parsing was interrupted due to scope exit token. "sv;

	public:
		// TODO: refactor return type, some kind of noexcept string_view
		virtual std::string_view what() const noexcept {
			return description;
		}
	};


	class help_requested : public parsing_aborted {
		static constexpr std::string_view description = "Command line parsing was interrupted due to help token. "sv;

	private:
		std::unique_ptr<help_context> cx;

	public:
		help_requested(help_requested&& other) noexcept = default;
		help_requested& operator=(help_requested&& other) noexcept = default;
		
		help_requested(std::unique_ptr<help_context>&& help_cx) : cx(std::move(help_cx)) {};


		// TODO: refactor return type, some kind of noexcept string_view
		virtual std::string_view what() const noexcept {
			return description;
		}

		help_context& get_help_context()& {
			return *(this->cx);
		}

		const help_context& get_help_context() const & {
			return *(this->cx);
		}

		[[nodiscard]]
		std::unique_ptr<help_context> get_help_context()&& {
			return std::move(this->cx);
		}
	};

	class envrironment_not_supported : public exception {
		static constexpr std::string_view description = "Environment is not supported. Environment is required to be UTF-8 compatible. "sv;

	public:
		// TODO: refactor return type, some kind of noexcept string_view
		std::string_view what() const noexcept override {
			return description;
		}
	};
}
