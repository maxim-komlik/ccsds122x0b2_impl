project(core CXX C)

set(CORE_SOURCE_FILES 
	"${REPO_SOURCE_DIR}dwt/bitmap.tpp"
	"${REPO_SOURCE_DIR}dwt/dwt.cpp"
	"${REPO_SOURCE_DIR}dwt/dwtcore.tpp"
	"${REPO_SOURCE_DIR}dwt/dwtscale.tpp"
	"${REPO_SOURCE_DIR}dwt/segment_assembly.cpp"
	"${REPO_SOURCE_DIR}bpe/bitplane.tpp"
	"${REPO_SOURCE_DIR}bpe/bpe.cpp"
	"${REPO_SOURCE_DIR}bpe/entropy_code.tpp"
	"${REPO_SOURCE_DIR}bpe/ibitwrapper.tpp"
	"${REPO_SOURCE_DIR}bpe/obitwrapper.tpp"
	"${REPO_SOURCE_DIR}bpe/symbolbuffer.cpp"
	"${REPO_SOURCE_DIR}bpe/symbolstream.cpp"
	"${REPO_SOURCE_DIR}bpe/symbol_code.tpp"
	"${REPO_SOURCE_DIR}core/compression.cpp"
	"${REPO_SOURCE_DIR}core/io.cpp"
	"${REPO_SOURCE_DIR}core/samples/routines.tpp"
	"${REPO_SOURCE_DIR}io/ccsds_protocol.cpp"
	"${REPO_SOURCE_DIR}io/endian.tpp"
	"${REPO_SOURCE_DIR}io/file_sink.tpp"
	"${REPO_SOURCE_DIR}io/io_data_registry.cpp"
	"${REPO_SOURCE_DIR}io/memory_sink.tpp"
	"${REPO_SOURCE_DIR}io/tasking.cpp"
	"${REPO_SOURCE_DIR}common/aligned_vector.tpp"
	)
	
	
set(CORE_HEADER_FILES 
	"${REPO_SOURCE_DIR}dwt/dwt.tpp"
	"${REPO_SOURCE_DIR}dwt/dwt.hpp"
	"${REPO_SOURCE_DIR}dwt/img_meta.hpp"
	"${REPO_SOURCE_DIR}dwt/utility.hpp"
	"${REPO_SOURCE_DIR}bpe/entropy_translate.hpp"
	"${REPO_SOURCE_DIR}bpe/entropy_types.hpp"
	"${REPO_SOURCE_DIR}dwt/segment_assembly.tpp"
	"${REPO_SOURCE_DIR}dwt/segment_assembly.hpp"
	"${REPO_SOURCE_DIR}bpe/bpe.tpp"
	"${REPO_SOURCE_DIR}bpe/bpe.hpp"
	"${REPO_SOURCE_DIR}bpe/symbolbuffer.hpp"
	"${REPO_SOURCE_DIR}bpe/symbolstream.hpp"
	"${REPO_SOURCE_DIR}bpe/symbol_translate.hpp"
	"${REPO_SOURCE_DIR}core/io.hpp"
	"${REPO_SOURCE_DIR}core/compression.hpp"
	"${REPO_SOURCE_DIR}io/bitfield.hpp"
	"${REPO_SOURCE_DIR}io/file_protocol.hpp"
	"${REPO_SOURCE_DIR}io/ccsds_protocol.hpp"
	"${REPO_SOURCE_DIR}io/headers.hpp"
	"${REPO_SOURCE_DIR}io/io_contexts.hpp"
	"${REPO_SOURCE_DIR}io/io_settings.hpp"
	"${REPO_SOURCE_DIR}io/io_data_registry.hpp"
	"${REPO_SOURCE_DIR}io/io_data_registry_fwd.hpp"
	"${REPO_SOURCE_DIR}io/memory_protocol.hpp"
	"${REPO_SOURCE_DIR}io/session_context.hpp"
	"${REPO_SOURCE_DIR}io/sink.hpp"
	"${REPO_SOURCE_DIR}io/tasking.hpp"
	"${REPO_SOURCE_DIR}common/compressor_types.hpp"
	"${REPO_SOURCE_DIR}common/core_types.hpp"
)

add_library(core STATIC
	${CORE_SOURCE_FILES}
	${CORE_HEADER_FILES}
	)

# target_sources(core
# 	PRIVATE
# 
# 	PUBLIC
# 	PRIVATE
# 	PUBLIC
# 	INTERFACE
# )


target_link_libraries(core
	PRIVATE 
		common
		constant
	)

target_include_directories(core 
	PRIVATE
		"${REPO_SOURCE_DIR}common"
		"${REPO_SOURCE_DIR}"
	)
