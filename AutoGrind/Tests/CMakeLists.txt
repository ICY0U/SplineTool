# Builds the engine-free detector and its checks on any platform:
#
#   cmake -S AutoGrind/Tests -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#   ctest --test-dir build -C Release --output-on-failure
#
# The detector in Source/AutoGrind/Private/Core is the same code the editor plugin compiles. Warnings are
# errors, at least as strict as Unreal's: shadowed names are errors in Unreal builds.
cmake_minimum_required(VERSION 3.16)
project(AutoGrindTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard (Unreal 5.4 builds plugins as C++20)")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(AUTOGRIND_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

file(GLOB AUTOGRIND_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../Source/AutoGrind/Private/Core/*.cpp")
add_library(autogrind_core STATIC ${AUTOGRIND_CORE_SOURCES})
target_include_directories(autogrind_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../Source/AutoGrind/Private/Core")

if(MSVC)
	set(AUTOGRIND_WARNINGS /W4 /WX /permissive- /w14456 /w14457 /w14458 /w14459)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
	set(AUTOGRIND_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Werror)
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		list(APPEND AUTOGRIND_WARNINGS -Wshadow-all -Wconversion -Wsign-conversion)
	endif()
endif()
target_compile_options(autogrind_core PRIVATE ${AUTOGRIND_WARNINGS})

if(AUTOGRIND_SANITIZE AND NOT MSVC)
	# Undefined behaviour stops the program, so a test cannot pass over it.
	set(AUTOGRIND_SANITIZERS -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
	add_compile_options(${AUTOGRIND_SANITIZERS})
	add_link_options(-fsanitize=address,undefined)
	target_compile_options(autogrind_core PRIVATE ${AUTOGRIND_SANITIZERS})
	target_link_options(autogrind_core PUBLIC -fsanitize=address,undefined)
endif()

foreach(Program regression benchmark drawing stress harness)
	add_executable(${Program} ${Program}.cpp)
	target_link_libraries(${Program} PRIVATE autogrind_core)
	target_compile_options(${Program} PRIVATE ${AUTOGRIND_WARNINGS})
endforeach()

# A compile and link check of the editor module against EditorCheck/, small stand-ins for the parts of the
# engine API it uses. It catches typos, wrong types, mismatched delegates and format strings, and missing
# definitions without Unreal; it does not prove the module builds against the engine (run_unreal.ps1 does).
option(AUTOGRIND_EDITOR_CHECK "Compile and link the editor module against stand-in engine headers (GCC or Clang)" OFF)
if(AUTOGRIND_EDITOR_CHECK)
	if(MSVC)
		message(FATAL_ERROR "AUTOGRIND_EDITOR_CHECK needs GCC or Clang")
	endif()
	set(EDITOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Source/AutoGrind/Private")
	set(FORWARD_DIR "${CMAKE_CURRENT_BINARY_DIR}/editor_check_include")
	file(GLOB EDITOR_SOURCES CONFIGURE_DEPENDS "${EDITOR_DIR}/*.cpp")
	file(GLOB EDITOR_FILES CONFIGURE_DEPENDS "${EDITOR_DIR}/*.cpp" "${EDITOR_DIR}/*.h")
	# Every engine header the module includes forwards to the stand-ins; generated headers are empty.
	foreach(File ${EDITOR_FILES})
		file(STRINGS "${File}" Includes REGEX "^#include \"[^\"]+\"")
		foreach(Line ${Includes})
			string(REGEX REPLACE "^#include \"([^\"]+)\".*" "\\1" Header "${Line}")
			if(EXISTS "${EDITOR_DIR}/${Header}")
				continue()
			endif()
			if(Header MATCHES "\\.generated\\.h$")
				file(WRITE "${FORWARD_DIR}/${Header}" "")
			else()
				file(WRITE "${FORWARD_DIR}/${Header}" "#pragma once\n#include \"UEStubAll.h\"\n")
			endif()
		endforeach()
	endforeach()
	add_executable(editor_check ${EDITOR_SOURCES} EditorCheck/Definitions.cpp)
	target_include_directories(editor_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/EditorCheck" "${FORWARD_DIR}" "${EDITOR_DIR}")
	target_link_libraries(editor_check PRIVATE autogrind_core)
	set_target_properties(editor_check PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
	# Unreal builds treat shadowed names as errors. Two Draw mode overrides are deliberately not marked
	# override: they are declared differently across engine versions.
	target_compile_options(editor_check PRIVATE -Wall -Wextra -Wshadow -Werror -Wno-unused-parameter -Wno-missing-field-initializers
		$<$<CXX_COMPILER_ID:Clang>:-Wno-inconsistent-missing-override>)
endif()

enable_testing()
add_test(NAME regression COMMAND regression)
add_test(NAME drawing COMMAND drawing)
# Every scene, also turned and moved off the origin.
add_test(NAME benchmark COMMAND benchmark -turn)
# Generous time limit: shared CI machines are slow.
add_test(NAME stress COMMAND stress 60)
if(AUTOGRIND_EDITOR_CHECK)
	add_test(NAME editor_check COMMAND editor_check)
endif()
