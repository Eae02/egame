#pragma once

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

#ifdef __EMSCRIPTEN__

#include <GLES3/gl32.h>
#include <GLES3/gl2ext.h>

#define GL_MAX_CLIP_DISTANCES GL_MAX_CLIP_DISTANCES_EXT
#define GL_CLIP_DISTANCE0 GL_CLIP_DISTANCE0_EXT

#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 35917
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 35918
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 35919
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 35916
#define GL_COMPRESSED_RED_RGTC1 36283
#define GL_COMPRESSED_RG_RGTC2 36285

#define EG_GLES

#else
#include <cstdint>
#include <cstddef>
#include <GL/gl3w.h>
#include <GL/glext.h>
#endif
