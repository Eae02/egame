cmake_minimum_required(VERSION 3.25)
project(EGame)

set(DEFAULT_ENABLE_VULKAN ON)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten" OR ${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(DEFAULT_ENABLE_VULKAN OFF)
endif()

option(EG_BUILD_ASSETGEN "Whether or not to build asset generator library" ON)
option(EG_BUILD_ASSETMAN "Whether or not to build asset manager utility" ON)
option(EG_BUILD_IMGUI "Whether or not to build imgui support library" ON)
option(EG_VULKAN "Whether or not to enable vulkan support." ${DEFAULT_ENABLE_VULKAN})
option(EG_WEBGPU "Whether or not to enable webgpu support." OFF)
option(EG_WEBGPU_TINT "Whether or not to use tint for webgpu shader transpilation." OFF)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMake)

set(BUILD_NAME ${CMAKE_BUILD_TYPE}-${CMAKE_SYSTEM_NAME})
set(OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Bin/${BUILD_NAME})

option(SPIRV_CROSS_ENABLE_TESTS "" OFF)
option(SPIRV_CROSS_CLI "" OFF)
option(SPIRV_CROSS_ENABLE_HLSL "" OFF)
option(SPIRV_CROSS_ENABLE_CPP "" OFF)
option(SPIRV_CROSS_ENABLE_REFLECT "" OFF)
option(SPIRV_CROSS_ENABLE_C_API "" OFF)
option(SPIRV_CROSS_FORCE_PIC "" ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Deps/spirv_cross/ SYSTEM)

if (EG_WEBGPU)
	set(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Deps/spirv_headers/")
	set(SPIRV_SKIP_EXECUTABLES ON)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Deps/spirv_tools/ SYSTEM)
endif()

set(YAML_BUILD_SHARED_LIBS OFF)
set(YAML_CPP_BUILD_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Deps/yaml-cpp/ SYSTEM)

target_compile_options(yaml-cpp PRIVATE -fPIC)

file(GLOB_RECURSE EGAME_SOURCE_FILES Src/EGame/*.cpp Src/EGame/*.mm)
file(GLOB_RECURSE ASSET_GEN_SOURCE_FILES Src/AssetGen/*.cpp)
file(GLOB_RECURSE ASSET_MAN_SOURCE_FILES Src/AssetMan/*.cpp)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	list(FILTER EGAME_SOURCE_FILES EXCLUDE REGEX "Src/EGame/Graphics/Metal/*")
	list(FILTER EGAME_SOURCE_FILES EXCLUDE REGEX ".*\\.mm")
endif()

if (NOT EG_WEBGPU)
	list(FILTER EGAME_SOURCE_FILES EXCLUDE REGEX "Src/EGame/Graphics/WebGPU/*")
endif()

if (NOT EG_VULKAN)
	list(FILTER EGAME_SOURCE_FILES EXCLUDE REGEX "Src/EGame/Graphics/Vulkan/*")
endif()

if (EG_WEBGPU_TINT)
	set(TINT_BUILD_CMD_TOOLS OFF)
	set(TINT_BUILD_WGSL_READER OFF)
	set(TINT_BUILD_GLSL_WRITER OFF)
	set(TINT_BUILD_HLSL_WRITER OFF)
	set(TINT_BUILD_MSL_WRITER OFF)
	set(TINT_BUILD_SPV_WRITER OFF)
	set(TINT_SPIRV_TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Deps/spirv_tools)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Deps/tint/ SYSTEM)
endif()

#Adds compile options for warnings
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(WARNING_FLAGS
		-Werror
		-Wall
		-Wextra
		-Wshadow
		-Wcast-qual
		-Wconversion
		-Woverlength-strings
		-Wpointer-arith
		-Wunused-local-typedefs
		-Wunused-result
		-Wvarargs
		-Wvla
		-Wstrict-aliasing
		-Wwrite-strings
		-Wno-unused-parameter
		-Wno-missing-field-initializers
		-Wno-cast-function-type
		-Wno-error=unused-value
		-Wno-error=unused-function
		-Wno-error=restrict #weird gcc bug
		$<$<COMPILE_LANGUAGE:CXX>:-Wconversion-null>
		$<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
	)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	set(WARNING_FLAGS /wd4250 /wd4251 /wd4068 /wd4996 /wd4275 /D_CRT_SECURE_NO_WARNINGS)
endif()

add_library(EGame SHARED ${EGAME_SOURCE_FILES} Deps/volk/volk.c Assets/DevFont.png.h Assets/DevFont.fnt.h)
add_executable(EGSandbox Src/Sandbox/Main.cpp)

target_compile_options(EGame PRIVATE ${WARNING_FLAGS})

if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	find_package(ZLIB REQUIRED)
	find_package(Threads REQUIRED)
	find_package(Freetype)
	find_package(OpenAL)

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
	
	target_include_directories(EGame SYSTEM PRIVATE
		${ZLIB_INCLUDE_DIRS}
		${SDL2_INCLUDE_DIRS}
	)
	
	target_link_options(EGame PRIVATE ${SDL2_LDFLAGS})
	target_compile_options(EGame PRIVATE ${SDL2_CFLAGS_OTHER})
	
	if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		target_link_libraries(EGame PRIVATE
			"-framework Cocoa"
			"-framework OpenAL"
			"-framework Metal"
			"-framework Foundation"
			"-framework QuartzCore"
		)
	endif()
	
	if (NOT ${OPENAL_FOUND})
		message(WARNING "OpenAL not found, audio support will be disabled")
		target_compile_options(EGame PRIVATE -DEG_NO_OPENAL)
	else()
		target_include_directories(EGame SYSTEM PRIVATE ${OPENAL_INCLUDE_DIR})
	endif()
	
	if (NOT ${FREETYPE_FOUND})
		message(WARNING "Freetype not found, freetype loading will be disabled")
		target_compile_options(EGame PRIVATE -DEG_NO_FREETYPE)
	else()
		target_include_directories(EGame SYSTEM PRIVATE ${FREETYPE_INCLUDE_DIRS})
	endif()
	
	if (EG_BUILD_ASSETGEN)
		find_package(nlohmann_json REQUIRED)
		pkg_check_modules(OGG_VORBIS REQUIRED ogg vorbis vorbisfile)
		
		add_library(EGameAssetGen SHARED ${ASSET_GEN_SOURCE_FILES})
		
		add_dependencies(EGameAssetGen EGame)
		target_link_libraries(EGameAssetGen PRIVATE
			EGame
			nlohmann_json::nlohmann_json
			${OGG_VORBIS_LIBRARIES}
		)
		
		target_compile_options(EGameAssetGen PRIVATE ${WARNING_FLAGS} -Wno-missing-declarations)
		
		target_include_directories(EGameAssetGen SYSTEM PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
			${CMAKE_CURRENT_SOURCE_DIR}/Deps/inc
			${OGG_VORBIS_INCLUDE_DIRS}
		)
		
		target_link_options(EGameAssetGen PRIVATE ${OGG_VORBIS_LDFLAGS})
		target_compile_options(EGameAssetGen PRIVATE ${OGG_VORBIS_CFLAGS_OTHER})
		
		set_target_properties(EGameAssetGen PROPERTIES
			CXX_STANDARD 20
			LIBRARY_OUTPUT_DIRECTORY ${OUT_DIR}
		)
	endif()
	
	if (EG_BUILD_ASSETMAN)
		add_executable(EGameAssetMan ${ASSET_MAN_SOURCE_FILES})
		
		add_dependencies(EGameAssetMan EGame)
		target_link_libraries(EGameAssetMan PRIVATE EGame)
		
		set_target_properties(EGameAssetMan PROPERTIES
			CXX_STANDARD 20
			RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}
		)
	endif()
endif()

target_link_libraries(EGame PUBLIC yaml-cpp::yaml-cpp)
target_compile_options(EGame PUBLIC -DEG_HAS_YAML_CPP)

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
	
	target_include_directories(EGameImGui SYSTEM PUBLIC
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
		spirv-cross-msl
		spirv-cross-util
		spirv-cross-core
)

target_include_directories(EGame PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Src/>)

target_include_directories(EGame SYSTEM
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/glm
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/stb
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/utfcpp/source
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/volk
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vma/include
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/vulkan-headers/include
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/metal-cpp/
		${CMAKE_CURRENT_SOURCE_DIR}/Deps/inc/
)

if (EG_VUKLAN)
	target_compile_options(EGame PRIVATE -DEG_ENABLE_VULKAN)
endif()

if (EG_WEBGPU)
	target_compile_options(EGame PRIVATE -DEG_ENABLE_WEBGPU)
	target_link_libraries(EGame PRIVATE SPIRV-Tools-opt)
endif()
if (EG_WEBGPU_TINT)
	target_link_libraries(EGame PRIVATE libtint)
	target_compile_options(EGame PRIVATE -DEG_ENABLE_TINT)
endif()

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
	set(EMCC_FLAGS "-lopenal -s ALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -s WASM=1 -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_ZLIB=1 -sFORCE_FILESYSTEM -sEXPORTED_RUNTIME_METHODS=cwrap -sFETCH -s DISABLE_DEPRECATED_FIND_EVENT_TARGET_BEHAVIOR=0")
	
	if (EG_WEBGPU)
		set(EMCC_FLAGS "${EMCC_FLAGS} -sUSE_WEBGPU")
	endif()
	
	if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
		set(EMCC_FLAGS "${EMCC_FLAGS} -gsource-map")
		target_compile_options(EGame PRIVATE -gsource-map)
		target_compile_options(EGSandbox PRIVATE -gsource-map)
	endif()
	
	target_link_libraries(EGame PUBLIC idbfs.js)
	
	set_target_properties(EGSandbox PROPERTIES OUTPUT_NAME game)
	
	target_compile_options(EGame PRIVATE -DEG_NO_FREETYPE -s USE_ZLIB=1 -Wno-sign-conversion -Wno-shorten-64-to-32)
	set_target_properties(EGame PROPERTIES LINK_FLAGS "${EMCC_FLAGS}")
	set_target_properties(EGSandbox PROPERTIES LINK_FLAGS "${EMCC_FLAGS}")
endif()
