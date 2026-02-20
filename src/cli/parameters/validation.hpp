#pragma once

#include <string>
#include <vector>
#include <utility>
#include <memory>

#include "cli.hpp"

namespace cli::validate {

class validation_context {
public:
	using error_t = std::u8string;
	using warning_t = std::u8string;

private:
	std::u8string category;
	std::vector<error_t> errors;
	std::vector<warning_t> warnings;

	std::vector<std::unique_ptr<validation_context>> nested_container;

public:
	validation_context(std::u8string_view category) : category(static_cast<std::u8string>(category)) {}
	validation_context(std::u8string&& category) noexcept : category(std::move(category)) {}

	void error(bool pred, error_t&& description) {
		if (!pred) {
			this->errors.push_back(std::move(description));
		}
	}

	void warning(bool check_pred, error_t&& description) {
		if (!check_pred) {
			this->warnings.push_back(std::move(description));
		}
	}

	const std::u8string& get_category() const noexcept {
		return this->category;
	}

	const std::vector<error_t>& get_errors() const noexcept {
		return this->errors;
	}

	const std::vector<warning_t>& get_warnings() const noexcept {
		return this->warnings;
	}

	bool has_nested() const {
		return !this->nested_container.empty();
	}

	bool has_scoped_errors() const {
		return !this->errors.empty();
	}

	bool has_nested_errors() const {	// ?
		bool result = false;

		for (ptrdiff_t i = 0; i < this->nested_size(); ++i) {
			result |= nested(i).has_errors();
		}

		return result;
	}

	bool has_errors() const {
		return this->has_scoped_errors() | has_nested_errors();
	}

	bool has_scoped_warnings() const {
		return !this->warnings.empty();
	}

	bool has_nested_warnings() const {
		bool result = false;

		for (ptrdiff_t i = 0; i < this->nested_size(); ++i) {
			result |= nested(i).has_warnings();
		}

		return result;
	}

	bool has_warnings() const {
		return this->has_scoped_warnings() | this->has_nested_warnings();
	}

	size_t nested_size() const {
		return this->nested_container.size();
	}

	validation_context& nested(ptrdiff_t i) {
		return *(this->nested_container[i]);
	}

	const validation_context& nested(ptrdiff_t i) const {
		return *(this->nested_container[i]);
	}

	validation_context& nest_context(validation_context&& other) {
		ptrdiff_t result = this->nested_container.size();

		return *(this->nested_container.emplace_back(
			std::make_unique<validation_context>(std::move(other))));
	}

	// TODO: iterator adapters for transparent pointer operations? begin/end?
};

}
