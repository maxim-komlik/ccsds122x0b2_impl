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
			const char* what() noexcept override;
		};
	};

	namespace io {
		class invalid_header_exception : public ccsds::exception {
			inline static const char description[] = "Segment header format violated. ";
		public:
			// TODO: refactor return type, some kind of noexcept string_view
			const char* what() noexcept override;
		};

		class truncated_header_exception : public ccsds::exception {
			inline static const char description[] = "Segment header is not complete. ";
			size_t expected_size = 1;
		public:
			truncated_header_exception(size_t size_hint) : expected_size(size_hint) {}

			// TODO: refactor return type, some kind of noexcept string_view
			const char* what() noexcept override;

			size_t get_expected_size() const noexcept { return this->expected_size; }
		};

		class incompatible_header_exception : public ccsds::io::invalid_header_exception {
			inline static const char description[] = "Segment header version is not compatible with "
				"requested header format. ";
		public:
			// TODO: refactor return type, some kind of noexcept string_view
			const char* what() noexcept override;
		};
	}
};
