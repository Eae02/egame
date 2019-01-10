cmake_minimum_required(VERSION 3.13)
project(EGame)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

file(GLOB_RECURSE EGAME_SOURCE_FILES Src/EGame/*.cpp Src/EGame/*.hpp)

#Adds compile options for warnings
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-Wall -Wextra -Wshadow -pedantic -Wno-unused-parameter -Wno-missing-field-initializers)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  add_compile_options(/wd4250 /wd4251 /wd4068 /wd4996 /wd4275 /D_CRT_SECURE_NO_WARNINGS)
endif()

add_library(EGame SHARED ${EGAME_SOURCE_FILES} Deps/gl3w.c)
add_executable(EGSandbox Src/Sandbox/Main.cpp)

target_link_libraries(EGame
	PUBLIC
		Threads::Threads
	PRIVATE
		SDL2::SDL2
		OpenGL::GL
		ZLIB::ZLIB
	)

target_include_directories(EGame
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Src/>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/Inc
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
)

target_include_directories(EGame SYSTEM
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/glm
)

target_compile_options(EGame
	PUBLIC
		-DGLM_FORCE_RADIANS -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL
	PRIVATE
		-DZLIB_CONST)

set(OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${CMAKE_BUILD_TYPE})

set_target_properties(EGame EGSandbox PROPERTIES
	CXX_STANDARD 17
	ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}
	LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
	RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set_target_properties(EGame PROPERTIES CXX_VISIBILITY_PRESET hidden)
	target_compile_options(EGame PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-include ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp>)
endif()

set_target_properties(EGame PROPERTIES DEFINE_SYMBOL "EG_BUILDING_LIB")

add_dependencies(EGSandbox EGame)
target_link_libraries(EGSandbox EGame)

export(TARGETS EGame FILE ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${CMAKE_BUILD_TYPE}/EGameTargets.cmake)
