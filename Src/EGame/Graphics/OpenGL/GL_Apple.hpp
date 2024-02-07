#pragma once

#define EG_GLES

#define GL_SILENCE_DEPRECATION
#define GL3_PROTOTYPES
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#include "../../Assert.hpp"

#define glInvalidateFramebuffer(...)
#define glMemoryBarrier(x)
#define glObjectLabel(a, b, c, d)

#define GL_COMPUTE_SHADER 0

#define glTextureView nullptr

#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148

#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_OTHER 0x8251

namespace eg::graphics_api::gl
{
typedef void(APIENTRYP PFNGLTEXSTORAGE2DMULTISAMPLEPROC)(
	GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height,
	GLboolean fixedsamplelocations);
typedef void(APIENTRYP PFNGLTEXSTORAGE3DMULTISAMPLEPROC)(
	GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth,
	GLboolean fixedsamplelocations);

typedef void(APIENTRY* GLDEBUGPROC)(
	GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message,
	const void* userParam);
typedef void(APIENTRYP PFNGLDEBUGMESSAGECONTROLPROC)(
	GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint* ids, GLboolean enabled);
typedef void(APIENTRYP PFNGLDEBUGMESSAGEINSERTPROC)(
	GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* buf);
typedef void(APIENTRYP PFNGLDEBUGMESSAGECALLBACKPROC)(GLDEBUGPROC callback, const void* userParam);
} // namespace eg::graphics_api::gl
