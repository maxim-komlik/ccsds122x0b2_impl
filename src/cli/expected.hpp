#pragma once

#include <variant>
#include <string>
#include <array>
#include <type_traits>
#include <utility>
#include <system_error> // ?

namespace cli {
	using namespace std::literals;

// something C++23 expected-like, until C++23 is available
struct unexpected {
	std::errc ec;
	// someday error library will be added, but for now put explainatory strings as is
	std::string_view reason;

private:
	static constexpr std::array default_reasons = { 
		std::pair{std::errc::invalid_argument, "Couldn't parse parameter. "sv},
		std::pair{std::errc::protocol_error, "Value constraints are not satisfied. "sv}
	};
	static constexpr std::string_view unknown_reason = "<unspecified error>"sv;

public:
	unexpected(std::errc condition) : ec(condition) {
		auto it = std::find_if(default_reasons.cbegin(), default_reasons.cend(),
			[condition](const auto& pair) -> bool {
				return pair.first == condition;
			});
		if (it == default_reasons.cend()) {
			reason = unknown_reason;
		} else {
			reason = it->second;
		}
	}

	unexpected(std::errc condition, std::string_view reason): ec(condition), reason(reason) {}
};

template <typename T>
class expected {
	static_assert(std::is_same_v<T, unexpected> == false);

private:
	std::variant<unexpected, T> wrapped_value;

public:
	expected(unexpected e) : wrapped_value(e) {}
	expected(T&& value) : wrapped_value(std::move(value)) {}

	explicit operator bool() const {
		return wrapped_value.index() > 0;
	}

	const T& value() const& {
		// check if has value and throw if not?
		return std::get<T>(wrapped_value);
	}

	T value()&& {
		return std::get<T>(std::move(wrapped_value));
	}

	const unexpected& error() const& {
		return std::get<unexpected>(wrapped_value);
	}

	unexpected error()&& {
		return std::get<unexpected>(std::move(wrapped_value));
	}
};

}
