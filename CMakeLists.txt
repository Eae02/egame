cmake_minimum_required(VERSION 3.12)
project(EGame)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(Freetype REQUIRED)
find_package(GLSLang REQUIRED)
find_package(Catch2 REQUIRED)

option(YAML_CPP_BUILD_TOOLS "" OFF)
option(YAML_CPP_BUILD_TESTS "" OFF)
add_subdirectory(Deps/yaml-cpp)

file(GLOB_RECURSE EGAME_SOURCE_FILES Src/EGame/*.cpp Src/EGame/*.hpp)
file(GLOB_RECURSE ASSET_GEN_SOURCE_FILES Src/AssetGen/*.cpp Src/AssetGen/*.hpp)
file(GLOB_RECURSE TEST_SOURCE_FILES Test/*.cpp)

set(SPIRV_CROSS_SOURCE_FILES
	Deps/spirv_cross/spirv_cross.cpp
	Deps/spirv_cross/spirv_parser.cpp
	Deps/spirv_cross/spirv_glsl.cpp
	Deps/spirv_cross/spirv_cross_parsed_ir.cpp
	Deps/spirv_cross/spirv_cfg.cpp)

#Adds compile options for warnings
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-Wall -Wextra -Wshadow -pedantic -Wno-unused-parameter -Wno-missing-field-initializers)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  add_compile_options(/wd4250 /wd4251 /wd4068 /wd4996 /wd4275 /D_CRT_SECURE_NO_WARNINGS)
endif()

add_library(EGame SHARED ${EGAME_SOURCE_FILES} ${SPIRV_CROSS_SOURCE_FILES} Deps/gl3w.c Deps/volk/volk.c)
add_library(EGameAssetGen SHARED ${ASSET_GEN_SOURCE_FILES})
add_executable(EGSandbox Src/Sandbox/Main.cpp)
add_executable(EGTest ${TEST_SOURCE_FILES})

target_link_libraries(EGame
	PUBLIC
		Threads::Threads
		yaml-cpp
	PRIVATE
		SDL2::SDL2
		OpenGL::GL
		ZLIB::ZLIB
		Freetype::Freetype
	)

target_link_libraries(EGameAssetGen PRIVATE EGame GLSLang::GLSLang)

target_include_directories(EGame
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Src/>
)

target_include_directories(EGameAssetGen PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
)

target_include_directories(EGame SYSTEM
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/glm
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/Inc
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/volk
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vma/src
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/utfcpp/source
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vulkan-headers/include
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/spirv_cross
)

target_compile_options(EGame
	PUBLIC
		-DGLM_FORCE_RADIANS -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL
	PRIVATE
		-DZLIB_CONST -DVMA_STATIC_VULKAN_FUNCTIONS=0 -DVK_NO_PROTOTYPES)

set(OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${CMAKE_BUILD_TYPE})

set_target_properties(EGame EGSandbox EGameAssetGen EGTest PROPERTIES
	CXX_STANDARD 17
	ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}
	LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
	RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	target_compile_options(yaml-cpp PRIVATE -fPIC)
	target_link_libraries(EGame PRIVATE dl)
	set_target_properties(EGame EGameAssetGen PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		INSTALL_RPATH "$ORIGIN"
		BUILD_WITH_INSTALL_RPATH TRUE
	)
	target_compile_options(EGame PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-include ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp> -msse4.1)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(EGame PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp>)
	target_link_libraries(EGame PRIVATE shlwapi.lib)
endif()

set_target_properties(EGame PROPERTIES DEFINE_SYMBOL "EG_BUILDING_LIB")

add_dependencies(EGSandbox EGame)
add_dependencies(EGameAssetGen EGame)
add_dependencies(EGTest EGameAssetGen EGame)
target_link_libraries(EGSandbox EGame)
target_link_libraries(EGTest Catch2::Catch2 EGameAssetGen EGame)

export(TARGETS EGame FILE ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${CMAKE_BUILD_TYPE}/EGameTargets.cmake)
