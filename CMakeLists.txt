cmake_minimum_required(VERSION 3.12)

project(catnap CXX)
# Test executable name
set(EXE_NAME kitten)

#===============================================================================

# Source: https://github.com/open-source-parsers/jsoncpp
# Docs: https://open-source-parsers.github.io/jsoncpp-docs/doxygen/index.html
#find_package(jsoncpp REQUIRED)

# Source:
# Docs:
find_package(nlohmann_json REQUIRED)

# Source:
# Docs:
find_package(GTest REQUIRED)

#===============================================================================

file(GLOB_RECURSE SRCS ${CMAKE_SOURCE_DIR}/lib${PROJECT_NAME}/*.cpp)
file(GLOB_RECURSE EXE_SRCS ${CMAKE_SOURCE_DIR}/${EXE_NAME}/*.cpp)

#===============================================================================

# Shareable library with the guts
add_library(${PROJECT_NAME} ${SRCS})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra)
target_link_libraries(${PROJECT_NAME}
	PUBLIC
	#jsoncpp_lib
	nlohmann_json::nlohmann_json
)
target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib${PROJECT_NAME}>
	$<INSTALL_INTERFACE:include/lib${PROJECT_NAME}>
)

# Test executable using the library
add_executable(${EXE_NAME} ${EXE_SRCS})
target_compile_features(${EXE_NAME} PRIVATE cxx_std_17)
target_compile_options(${EXE_NAME} PRIVATE -Wall -Wextra)
target_link_libraries(${EXE_NAME} PRIVATE
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/${EXE_NAME}>
)
#===============================================================================

enable_testing()

file(GLOB_RECURSE TESTS ${CMAKE_SOURCE_DIR}/test/*.cpp)

foreach(test_src ${TESTS})
	# Get the name without extension or directory
	get_filename_component(test_name ${test_src} NAME_WLE)
	# Message for the message gods
	message(STATUS "Adding test ${test_name}")
	# Build the test into an exe
	add_executable(${test_name} ${test_src})
	# Link it with GTests and main library
	target_link_libraries(${test_name}
		GTest::gtest
		GTest::gtest_main
		${PROJECT_NAME}
	)
	# Add it to the list for ctest to handle
	add_test(NAME ${test_name} COMMAND ${test_name})
endforeach(test_src)