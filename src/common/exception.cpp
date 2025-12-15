#include "exception.hpp"

using namespace ccsds;

const char* exception::what() noexcept {
	return exception::description;
}

const char* bpe::byte_limit_exception::what() noexcept {
	return byte_limit_exception::description;
}