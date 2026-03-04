project(common CXX)

add_library(common STATIC)

target_sources(common
	PRIVATE
		"${REPO_SOURCE_DIR}common/exception.cpp"
	PUBLIC
		"${REPO_SOURCE_DIR}common/exception.hpp"
		"${REPO_SOURCE_DIR}common/utility.hpp"
		"${REPO_SOURCE_DIR}common/meta.hpp"
	)

target_include_directories(common 
	PUBLIC
		"${REPO_SOURCE_DIR}common"
		"${REPO_SOURCE_DIR}"
	)

	
add_library(constant INTERFACE)

target_sources(constant
	INTERFACE
		"${REPO_SOURCE_DIR}common/constant.hpp"
		"${REPO_SOURCE_DIR}bpe/constant.hpp"
		"${REPO_SOURCE_DIR}dwt/constant.hpp"
		"${REPO_SOURCE_DIR}io/constant.hpp"
	)

target_include_directories(constant 
	INTERFACE 
		"${REPO_SOURCE_DIR}"
	)
