project(core_test CXX C)

enable_testing()

set(TEST_SOURCE_DIR "${REPO_SOURCE_DIR}test/")

add_executable(core_test)

target_sources(core_test
	PRIVATE
		"${TEST_SOURCE_DIR}compliance.cpp"
		"${TEST_SOURCE_DIR}extreme.cpp"
		"${TEST_SOURCE_DIR}smoke.cpp"
		"${TEST_SOURCE_DIR}static.cpp"
		"${TEST_SOURCE_DIR}test_utils.tpp"
		"${TEST_SOURCE_DIR}functional/computation_flow.cpp"
		"${TEST_SOURCE_DIR}functional/dwt_fragments.cpp"
	)

target_link_libraries(core_test
	PRIVATE
		GTest::gtest_main
		core
		common
	)
