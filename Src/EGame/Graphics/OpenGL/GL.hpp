#pragma once

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

#define OPENGL_LEVEL_WEBGL 0
#define OPENGL_LEVEL_WEBGL 0

#include <cstddef>
#include <cstdint>

#if defined(__EMSCRIPTEN__)
#include "GL_Emscripten.hpp"
#elif defined(__APPLE__)
#include "GL_Apple.hpp"
#else
#include <GL/glcorearb.h>
#include <GL/glext.h>
#endif

#ifndef __EMSCRIPTEN__
namespace eg::graphics_api::gl
{
// Defined in PlatformSpecific_DesktopGL.cpp
#define GL_FUNC(name, proc) extern proc name;
#define GL_FUNC_OPT(name, proc) extern proc name;
#include "DesktopGLFunctions.inl"
#undef GL_FUNC
#undef GL_FUNC_OPT
} // namespace eg::graphics_api::gl
#endif
