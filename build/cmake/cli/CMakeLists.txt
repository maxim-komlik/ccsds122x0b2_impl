project(cli CXX)

set(CLI_SOURCE_DIR "${REPO_SOURCE_DIR}cli/")
set(CLI_EXECUTABLE_NAME "ccsds122x0b2")

add_executable(cli)
target_sources(cli
	PRIVATE
		"${CLI_SOURCE_DIR}cli.cpp"
		"${CLI_SOURCE_DIR}contextual_parser.tpp"
		"${CLI_SOURCE_DIR}expected.hpp"
		"${CLI_SOURCE_DIR}file_utility.cpp"
		"${CLI_SOURCE_DIR}help_context.hpp"
		"${CLI_SOURCE_DIR}commands/compress.cpp"
		"${CLI_SOURCE_DIR}commands/core_environment.cpp"
		"${CLI_SOURCE_DIR}commands/core_environment.hpp"
		"${CLI_SOURCE_DIR}commands/core_environment.tpp"
		"${CLI_SOURCE_DIR}commands/load_image.tpp"
		"${CLI_SOURCE_DIR}parameters/parameters.cpp"
		"${CLI_SOURCE_DIR}parameters/root.cpp"
		"${CLI_SOURCE_DIR}parameters/validation.hpp"
		"${CLI_SOURCE_DIR}parameters/compress/validation.cpp"
		"${CLI_SOURCE_DIR}parameters/restore/validation.cpp"

	PRIVATE
		"${CLI_SOURCE_DIR}cli.hpp"
		"${CLI_SOURCE_DIR}file_utility.hpp"
		"${CLI_SOURCE_DIR}parameters_context.hpp"
		"${CLI_SOURCE_DIR}utility.hpp"
		"${CLI_SOURCE_DIR}meta.hpp"
		"${CLI_SOURCE_DIR}commands/compress.hpp"
		"${CLI_SOURCE_DIR}commands/restore.hpp"
		"${CLI_SOURCE_DIR}exception/exception.hpp"
		"${CLI_SOURCE_DIR}parameters/compress.hpp"
		"${CLI_SOURCE_DIR}parameters/parameters.hpp"
		"${CLI_SOURCE_DIR}parameters/restore.hpp"
		"${CLI_SOURCE_DIR}parameters/root.hpp"
		"${CLI_SOURCE_DIR}parameters/compress/parameters.hpp"
		"${CLI_SOURCE_DIR}parameters/compress/validation.hpp"
		"${CLI_SOURCE_DIR}parameters/restore/parameters.hpp"
		"${CLI_SOURCE_DIR}parameters/restore/validation.hpp"
		"${CLI_SOURCE_DIR}parsers/command_parser.hpp"
		"${CLI_SOURCE_DIR}parsers/dummy_parser.hpp"
		"${CLI_SOURCE_DIR}parsers/enum_parser.hpp"
		"${CLI_SOURCE_DIR}parsers/flag_parser.hpp"
		"${CLI_SOURCE_DIR}parsers/general.hpp"
		"${CLI_SOURCE_DIR}parsers/integer_parser.hpp"
		"${CLI_SOURCE_DIR}parsers/path_parser.hpp"
	)

set_source_files_properties(
	"${CLI_SOURCE_DIR}commands/compress.cpp"
	PROPERTIES
		COMPILE_OPTIONS "/bigobj"
		# synonims for:
		# 	gcc: -mbig-obj
		# 	clang: -bigobj (same should work)
)

target_include_directories(cli
	PRIVATE
		${CLI_SOURCE_DIR})

target_link_libraries(cli
	PRIVATE
		core
		common
		constant
	)

set_target_properties(cli
	PROPERTIES
		OUTPUT_NAME ${CLI_EXECUTABLE_NAME})


# embed manifest to enforce utf-8 environment
if (${WIN32})
	set(CLI_UTF8_MANIFEST_FILE "${REPO_BUILD_DIR}utf8.manifest")
	# add_custom_command(TARGET cli
	# 	COMMAND "mt.exe -manifest ${CLI_UTF8_MANIFEST_FILE} -outputresource:" $<TARGET_FILE:tgt>)
	target_sources(cli
		PRIVATE
			${CLI_UTF8_MANIFEST_FILE})
endif()
