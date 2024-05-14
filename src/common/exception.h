#pragma once

namespace ccsds {
	class exception {
		inline static const char description[] = "General CCSDS error. ";
	public:
		virtual ~exception() = default;
		exception() noexcept = default;
		exception(const exception& other) noexcept = default;
		exception& operator=(const exception& other) noexcept = default;

		// TODO: refactor return type, some kind of noexcept string_view
		virtual const char* what() noexcept;
	};

	namespace bpe {
		class byte_limit_exception : public ccsds::exception {
			inline static const char description[] = "Bit IO buffer byte limit reached. ";
		public:
			~byte_limit_exception() = default;
			byte_limit_exception() noexcept = default;
			byte_limit_exception(const byte_limit_exception& other) noexcept = default;
			byte_limit_exception& operator=(const byte_limit_exception& other) noexcept = default;

			// TODO: refactor return type, some kind of noexcept string_view
			const char* what() noexcept;
		};
	};
};
