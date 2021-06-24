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

add_library(yaml-cpp STATIC ${YAML_SOURCE_FILES})
add_library(EGame SHARED ${EGAME_SOURCE_FILES} ${SPIRV_CROSS_SOURCE_FILES} Deps/gl3w.c Deps/volk/volk.c Assets/DevFont.png.h Assets/DevFont.fnt.h)
add_executable(EGSandbox Src/Sandbox/Main.cpp)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	find_package(ZLIB REQUIRED)
	find_package(Threads REQUIRED)
	find_package(Freetype)
	find_package(PkgConfig REQUIRED)
	find_package(OpenAL REQUIRED)
	
	pkg_check_modules(SDL2 REQUIRED sdl2)
	
	target_link_libraries(EGame
	PUBLIC
		${CMAKE_THREAD_LIBS_INIT}
	PRIVATE
		${SDL2_LIBRARIES}
		${ZLIB_LIBRARIES}
		${CMAKE_DL_LIBS}
	)
	
	target_include_directories(EGame PRIVATE
		${FREETYPE_INCLUDE_DIRS}
		${ZLIB_INCLUDE_DIRS}
		${SDL2_INCLUDE_DIRS}
		${OPENAL_INCLUDE_DIR}
	)
	
	if (NOT ${FREETYPE_FOUND})
		message("Freetype not found, freetype loading will be disabled")
		target_compile_options(EGame PRIVATE -DEG_NO_FREETYPE)
	else()
		target_include_directories(EGame PRIVATE
			${FREETYPE_INCLUDE_DIRS}
			${ZLIB_INCLUDE_DIRS}
			${SDL2_INCLUDE_DIRS}
		)
	endif()
	
	target_compile_options(EGame PRIVATE ${SDL2_CFLAGS_OTHER} -msse4.1)
	
	if (BUILD_ASSETGEN)
		find_package(GLSLang REQUIRED)
		pkg_check_modules(OGG_VORBIS REQUIRED ogg vorbis vorbisfile)
		
		add_library(EGameAssetGen SHARED ${ASSET_GEN_SOURCE_FILES})
		
		add_dependencies(EGameAssetGen EGame)
		target_link_libraries(EGameAssetGen PRIVATE
			EGame
			GLSLang::GLSLang
			${OGG_VORBIS_LIBRARIES}
		)
		
		target_include_directories(EGameAssetGen PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/Inc
			${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
			${OGG_VORBIS_INCLUDE_DIRS}
		)
		
		set_target_properties(EGameAssetGen PROPERTIES
			CXX_STANDARD 20
			LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
		)
	endif()
endif()

target_link_libraries(EGame PUBLIC yaml-cpp)

target_include_directories(yaml-cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Deps/yaml-cpp/include)

target_include_directories(EGame
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Src/>
)

target_include_directories(EGame SYSTEM
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/glm
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/yaml-cpp/include
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/Inc
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/volk
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vma/include
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/utfcpp/source
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vulkan-headers/include
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/spirv_cross
)

target_compile_options(EGame
	PUBLIC
		-DGLM_FORCE_RADIANS -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL
	PRIVATE
		-DZLIB_CONST -DVMA_STATIC_VULKAN_FUNCTIONS=0 -DVK_NO_PROTOTYPES)

set_target_properties(yaml-cpp PROPERTIES CXX_STANDARD 20)

set_target_properties(EGame EGSandbox PROPERTIES
	CXX_STANDARD 20
	ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}
	LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
	RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}
)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set_source_files_properties(
		Src/EGame/Collision.cpp
		Src/AssetGen/STB.cpp
		Src/AssetGen/Texture2DWriter.cpp
		Src/EGame/STB.cpp
	PROPERTIES
		COMPILE_FLAGS "-O3"
	)
	set_source_files_properties(
		Src/AssetGen/ShaderResource.cpp
	PROPERTIES
		COMPILE_FLAGS "-std=c++2a"
	)
	target_compile_options(yaml-cpp PRIVATE -fPIC)
endif()

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(EGame PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		INSTALL_RPATH "$ORIGIN"
		BUILD_WITH_INSTALL_RPATH TRUE
	)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_link_libraries(EGame PRIVATE shlwapi.lib)
endif()

set_target_properties(EGame PROPERTIES DEFINE_SYMBOL "EG_BUILDING_LIB")

add_dependencies(EGSandbox EGame)
target_link_libraries(EGSandbox EGame)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	set(EMCC_FLAGS "-s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_ZLIB=1 -s FORCE_FILESYSTEM=1 -s FETCH=1")
	
	if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		set(EMCC_FLAGS "${EMCC_FLAGS} -g4")
		target_compile_options(EGame PRIVATE -g4)
		target_compile_options(EGSandbox PRIVATE -g4)
	endif()
	
	set_target_properties(EGSandbox PROPERTIES OUTPUT_NAME game)
	
	target_compile_options(EGame PRIVATE -DEG_NO_VULKAN -s USE_ZLIB=1)
	set_target_properties(EGame PROPERTIES LINK_FLAGS "${EMCC_FLAGS}")
	set_target_properties(EGSandbox PROPERTIES LINK_FLAGS "-s EXTRA_EXPORTED_RUNTIME_METHODS=['cwrap'] -s EXPORTED_FUNCTIONS=['_WebMain'] ${EMCC_FLAGS}")
endif()

export(TARGETS EGame yaml-cpp FILE ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${BUILD_NAME}/EGameTargets.cmake)
