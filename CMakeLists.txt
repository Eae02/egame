cmake_minimum_required(VERSION 3.12)
project(EGame)

option(BUILD_ASSETGEN "Whether or not to build asset generator library. The asset generator requires glslang." ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

set(BUILD_NAME ${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME})
set(OUT_DIR ${CMAKE_SOURCE_DIR}/Bin/${BUILD_NAME})

file(GLOB_RECURSE YAML_SOURCE_FILES Deps/yaml-cpp/src/*.cpp)

file(GLOB_RECURSE EGAME_SOURCE_FILES Src/EGame/*.cpp Src/EGame/*.hpp)
file(GLOB_RECURSE ASSET_GEN_SOURCE_FILES Src/AssetGen/*.cpp Src/AssetGen/*.hpp)

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

add_library(EGameObj OBJECT ${EGAME_SOURCE_FILES} ${SPIRV_CROSS_SOURCE_FILES} Deps/gl3w.c Deps/volk/volk.c Assets/DevFont.png.h Assets/DevFont.fnt.h)
set_property(TARGET EGameObj PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(yaml-cpp STATIC ${YAML_SOURCE_FILES})
set_property(TARGET yaml-cpp PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(EGameShared SHARED $<TARGET_OBJECTS:EGameObj>)
add_library(EGameStatic STATIC $<TARGET_OBJECTS:EGameObj>)
add_executable(EGSandbox Src/Sandbox/Main.cpp)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	find_package(ZLIB REQUIRED)
	find_package(Threads REQUIRED)
	find_package(Freetype)
	find_package(PkgConfig REQUIRED)
	
	pkg_check_modules(SDL2 REQUIRED sdl2)
	
	target_link_libraries(EGameObj
	PUBLIC
		${CMAKE_THREAD_LIBS_INIT}
	PRIVATE
		${SDL2_LIBRARIES}
		${ZLIB_LIBRARIES}
		${CMAKE_DL_LIBS}
	)
	
	target_include_directories(EGameObj PRIVATE
		${FREETYPE_INCLUDE_DIRS}
		${ZLIB_INCLUDE_DIRS}
		${SDL2_INCLUDE_DIRS}
	)
	
	if (NOT ${FREETYPE_FOUND})
		message("Freetype not found, freetype loading will be disabled")
		target_compile_options(EGameObj PRIVATE -DEG_NO_FREETYPE)
	else()
		target_include_directories(EGameObj PRIVATE
			${FREETYPE_INCLUDE_DIRS}
			${ZLIB_INCLUDE_DIRS}
			${SDL2_INCLUDE_DIRS}
		)
	endif()
	
	target_compile_options(EGameObj PRIVATE ${SDL2_CFLAGS_OTHER} -msse4.1)
	
	if (BUILD_ASSETGEN)
		find_package(GLSLang REQUIRED)
		
		add_library(EGameAssetGen SHARED ${ASSET_GEN_SOURCE_FILES})
		
		add_dependencies(EGameAssetGen EGameShared)
		target_link_libraries(EGameAssetGen PRIVATE EGameShared GLSLang::GLSLang)
		
		target_include_directories(EGameAssetGen PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Inc
			${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
		)
		
		set_target_properties(EGameAssetGen PROPERTIES
			CXX_STANDARD 17
			LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
		)
	endif()
endif()

target_link_libraries(EGameShared PUBLIC yaml-cpp EGameObj)
target_link_libraries(EGameStatic PUBLIC yaml-cpp EGameObj)

target_include_directories(yaml-cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Deps/yaml-cpp/include)

target_include_directories(EGameObj PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Src/>)

target_include_directories(EGameObj SYSTEM
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/glm
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/yaml-cpp/include
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/Inc
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/volk
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vma/src
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/utfcpp/source
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vulkan-headers/include
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/spirv_cross
)

set(PUBLIC_COMPILE_OPTIONS -DGLM_FORCE_RADIANS -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL)

target_compile_options(EGameObj
	PUBLIC ${PUBLIC_COMPILE_OPTIONS}
	PRIVATE -DZLIB_CONST -DVMA_STATIC_VULKAN_FUNCTIONS=0 -DVK_NO_PROTOTYPES
)

target_compile_options(EGameShared PUBLIC ${PUBLIC_COMPILE_OPTIONS})
target_compile_options(EGameStatic PUBLIC ${PUBLIC_COMPILE_OPTIONS})

set_target_properties(yaml-cpp PROPERTIES CXX_STANDARD 17)

set_target_properties(EGameShared EGameStatic PROPERTIES OUTPUT_NAME EGame)

set_target_properties(EGameObj EGameShared EGameStatic EGSandbox PROPERTIES
	CXX_STANDARD 17
	ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}
	LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
	RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set_source_files_properties(Src/EGame/Collision.cpp PROPERTIES COMPILE_FLAGS "-O3")
	target_compile_options(EGameObj PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-include ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp>)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(EGameObj PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/Inc/Common.hpp>)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(EGameObj PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		INSTALL_RPATH "$ORIGIN"
		BUILD_WITH_INSTALL_RPATH TRUE
	)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_link_libraries(EGameObj PRIVATE shlwapi.lib)
endif()

set_target_properties(EGameObj PROPERTIES DEFINE_SYMBOL "EG_BUILDING_LIB")

add_dependencies(EGSandbox EGameShared)
target_link_libraries(EGSandbox EGameShared)

export(TARGETS EGameShared EGameStatic EGameObj yaml-cpp FILE ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${BUILD_NAME}/EGameTargets.cmake)
