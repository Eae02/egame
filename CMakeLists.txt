cmake_minimum_required(VERSION 3.12)
project(EGame)

option(EG_BUILD_ASSETGEN "Whether or not to build asset generator library. The asset generator requires glslang." ON)
option(EG_BUILD_ASSETMAN "Whether or not to build asset manager utility" ON)
option(EG_BUILD_IMGUI "Whether or not to build imgui support library" ON)
option(EG_INCLUDE_YAML_CPP "Whether or not to link to the yaml-cpp library." ON)
option(EG_VULKAN "Whether or not to enable vulkan support." ON)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

set(BUILD_NAME ${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME})
set(OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${BUILD_NAME})

option(SPIRV_CROSS_ENABLE_TESTS "" OFF)
option(SPIRV_CROSS_CLI "" OFF)
option(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS "" ON)
option(SPIRV_CROSS_ENABLE_HLSL "" OFF)
option(SPIRV_CROSS_ENABLE_MSL "" OFF)
option(SPIRV_CROSS_ENABLE_CPP "" OFF)
option(SPIRV_CROSS_ENABLE_REFLECT "" OFF)
option(SPIRV_CROSS_ENABLE_C_API "" OFF)
option(SPIRV_CROSS_FORCE_PIC "" ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Deps/spirv_cross/)

file(GLOB_RECURSE EGAME_SOURCE_FILES Src/EGame/*.cpp Src/EGame/*.hpp)
file(GLOB_RECURSE ASSET_GEN_SOURCE_FILES Src/AssetGen/*.cpp Src/AssetGen/*.hpp)
file(GLOB_RECURSE ASSET_MAN_SOURCE_FILES Src/AssetMan/*.cpp Src/AssetMan/*.hpp)

#Adds compile options for warnings
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-Wall -Wextra -Wshadow -pedantic -Wno-unused-parameter -Wno-missing-field-initializers -Werror=switch)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  add_compile_options(/wd4250 /wd4251 /wd4068 /wd4996 /wd4275 /D_CRT_SECURE_NO_WARNINGS)
endif()

add_library(EGame SHARED ${EGAME_SOURCE_FILES} Deps/gl3w.c Deps/volk/volk.c Assets/DevFont.png.h Assets/DevFont.fnt.h)
add_executable(EGSandbox Src/Sandbox/Main.cpp)

target_compile_options(EGame PRIVATE -Wconversion)

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	find_package(ZLIB REQUIRED)
	find_package(Threads REQUIRED)
	find_package(Freetype)
	find_package(OpenAL)
	
	if (EG_INCLUDE_YAML_CPP)
		find_package(yaml-cpp)
		if (NOT ${yaml-cpp_FOUND})
			message("Yaml-cpp was not disabled but was not found")
		endif()
	endif()
	
	find_package(PkgConfig REQUIRED)
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
		${ZLIB_INCLUDE_DIRS}
		${SDL2_INCLUDE_DIRS}
	)
	
	if (NOT ${OPENAL_FOUND})
		message(WARNING "OpenAL not found, audio support will be disabled")
		target_compile_options(EGame PRIVATE -DEG_NO_OPENAL)
	else()
		target_include_directories(EGame PRIVATE ${OPENAL_INCLUDE_DIR})
	endif()
	
	if (NOT ${FREETYPE_FOUND})
		message(WARNING "Freetype not found, freetype loading will be disabled")
		target_compile_options(EGame PRIVATE -DEG_NO_FREETYPE)
	else()
		target_include_directories(EGame PRIVATE ${FREETYPE_INCLUDE_DIRS})
	endif()
	
	target_compile_options(EGame PRIVATE ${SDL2_CFLAGS_OTHER} -msse4.1)
	
	if (EG_BUILD_ASSETGEN)
		find_package(GLSLang REQUIRED)
		find_package(nlohmann_json REQUIRED)
		pkg_check_modules(OGG_VORBIS REQUIRED ogg vorbis vorbisfile)
		
		add_library(EGameAssetGen SHARED ${ASSET_GEN_SOURCE_FILES})
		
		add_dependencies(EGameAssetGen EGame)
		target_link_libraries(EGameAssetGen PRIVATE
			EGame
			GLSLang::GLSLang
			nlohmann_json::nlohmann_json
			${OGG_VORBIS_LIBRARIES}
		)
		
		target_include_directories(EGameAssetGen SYSTEM PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/Inc
			${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
			${OGG_VORBIS_INCLUDE_DIRS}
		)
		
		set_target_properties(EGameAssetGen PROPERTIES
			CXX_STANDARD 20
			LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
		)
	endif()
	
	if (EG_BUILD_ASSETMAN)
		add_executable(EGameAssetMan ${ASSET_MAN_SOURCE_FILES})
		
		add_dependencies(EGameAssetMan EGame)
		target_link_libraries(EGameAssetMan PRIVATE EGame)
		
		target_include_directories(EGameAssetMan SYSTEM PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/Inc
		)
		
		set_target_properties(EGameAssetMan PROPERTIES
			CXX_STANDARD 20
			RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}
		)
	endif()
endif()

if (${yaml-cpp_FOUND})
	target_link_libraries(EGame PUBLIC yaml-cpp)
	target_compile_options(EGame PUBLIC -DEG_HAS_YAML_CPP)
endif()

if (${EG_BUILD_IMGUI})
	add_library(EGameImGui STATIC
		Deps/imgui/imgui.cpp
		Deps/imgui/imgui_demo.cpp
		Deps/imgui/imgui_draw.cpp
		Deps/imgui/imgui_widgets.cpp
		Deps/imgui/imgui_tables.cpp
		Deps/imgui/misc/cpp/imgui_stdlib.cpp
		Src/EGameImGui/EGameImGui.cpp
	)
	
	target_compile_options(EGameImGui PUBLIC
		-DIMGUI_USER_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/Src/EGameImGui/ImGuiConfig.hpp"
		-DEG_HAS_IMGUI)
	
	target_include_directories(EGameImGui PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Src/EGameImGui/>
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/imgui/
	)
	
	set_target_properties(EGameImGui PROPERTIES
		CXX_STANDARD 20
		ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR}
	)
	
	add_dependencies(EGameImGui EGame)
	target_link_libraries(EGameImGui EGame)
	
	add_dependencies(EGSandbox EGameImGui)
	target_link_libraries(EGSandbox EGameImGui)
endif()

target_link_libraries(EGame
	PRIVATE
		spirv-cross-glsl
		spirv-cross-util
		spirv-cross-core
)

target_include_directories(EGame PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Src/>)

target_include_directories(EGame SYSTEM
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/glm
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/Inc
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/volk
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vma/include
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/utfcpp/source
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vulkan-headers/include
)

target_compile_options(EGame
	PUBLIC
		-DGLM_FORCE_RADIANS -DGLM_FORCE_CTOR_INIT -DGLM_ENABLE_EXPERIMENTAL
	PRIVATE
		-DZLIB_CONST -DVMA_STATIC_VULKAN_FUNCTIONS=0 -DVK_NO_PROTOTYPES)

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
	set(EMCC_FLAGS "-lopenal -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_ZLIB=1 -s FORCE_FILESYSTEM=1 -s FETCH=1 -s DISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR=0")
	
	if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		set(EMCC_FLAGS "${EMCC_FLAGS} -gsource-map")
		target_compile_options(EGame PRIVATE -gsource-map)
		target_compile_options(EGSandbox PRIVATE -gsource-map)
	endif()
	
	set_target_properties(EGSandbox PROPERTIES OUTPUT_NAME game)
	
	target_compile_options(EGame PRIVATE -DEG_NO_VULKAN -DEG_NO_FREETYPE -s USE_ZLIB=1 -Wno-sign-conversion -Wno-shorten-64-to-32)
	set_target_properties(EGame PROPERTIES LINK_FLAGS "${EMCC_FLAGS}")
	set_target_properties(EGSandbox PROPERTIES LINK_FLAGS "-s EXPORTED_RUNTIME_METHODS=['cwrap'] -s EXPORTED_FUNCTIONS=['_WebMain'] ${EMCC_FLAGS}")
elseif(NOT ${EG_VULKAN})
	target_compile_options(EGame PRIVATE -DEG_NO_VULKAN)
endif()

export(TARGETS EGame FILE ${CMAKE_CURRENT_SOURCE_DIR}/CMake/${BUILD_NAME}/EGameTargets.cmake)
