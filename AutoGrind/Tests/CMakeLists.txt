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
	add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
	add_link_options(-fsanitize=address,undefined)
	target_compile_options(autogrind_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
	target_link_options(autogrind_core PUBLIC -fsanitize=address,undefined)
endif()

foreach(Program regression benchmark drawing stress harness)
	add_executable(${Program} ${Program}.cpp)
	target_link_libraries(${Program} PRIVATE autogrind_core)
	target_compile_options(${Program} PRIVATE ${AUTOGRIND_WARNINGS})
endforeach()

enable_testing()
add_test(NAME regression COMMAND regression)
add_test(NAME drawing COMMAND drawing)
# Every scene, also turned and moved off the origin.
add_test(NAME benchmark COMMAND benchmark -turn)
# Generous time limit: shared CI machines are slow.
add_test(NAME stress COMMAND stress 60)
