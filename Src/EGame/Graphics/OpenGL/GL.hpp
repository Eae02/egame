#pragma once

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

#ifdef __EMSCRIPTEN__

#include <GLES3/gl32.h>
#include <GLES3/gl2ext.h>

#define glObjectLabel(a, b, c, d)
#define glMemoryBarrier(x)
#define glTextureView nullptr

#define GL_MAX_CLIP_DISTANCES GL_MAX_CLIP_DISTANCES_EXT
#define GL_CLIP_DISTANCE0 GL_CLIP_DISTANCE0_EXT

#define GL_COMPRESSED_RED_RGTC1 36283
#define GL_COMPRESSED_RG_RGTC2 36285

#define EG_GLES

#else
#include <cstdint>
#include <cstddef>
#include <GL/glcorearb.h>
#include <GL/glext.h>
#include <GL/gl.h>

namespace eg::graphics_api::gl
{
	//Defined in PlatformSpecific_DesktopGL.cpp
	#define GL_FUNC(name, proc) extern proc name;
	#define GL_FUNC_OPT(name, proc) extern proc name;
	#include "DesktopGLFunctions.inl"
	#undef GL_FUNC
	#undef GL_FUNC_OPT
}

#endif
