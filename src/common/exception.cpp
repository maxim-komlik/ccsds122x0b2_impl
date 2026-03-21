#include "exception.hpp"

using namespace ccsds;

const char* exception::what() const noexcept {
	return exception::description;
}

const char* bpe::byte_limit_exception::what() const noexcept {
	return byte_limit_exception::description;
}

const char* io::invalid_header_exception::what() const noexcept {
	return invalid_header_exception::description;
}

const char* io::truncated_header_exception::what() const noexcept {
	return truncated_header_exception::description;
}

const char* io::incompatible_header_exception::what() const noexcept {
	return incompatible_header_exception::description;
}
