#include "exception.hpp"

using namespace ccsds;

const char* exception::what() noexcept {
	return exception::description;
}

const char* bpe::byte_limit_exception::what() noexcept {
	return byte_limit_exception::description;
}

const char* io::invalid_header_exception::what() noexcept {
	return invalid_header_exception::description;
}

const char* io::truncated_header_exception::what() noexcept {
	return truncated_header_exception::description;
}

const char* io::incompatible_header_exception::what() noexcept {
	return incompatible_header_exception::description;
}
