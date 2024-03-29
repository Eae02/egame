#ifndef __EMSCRIPTEN__

#include "../../Assert.hpp"
#include "../../String.hpp"
#include "Framebuffer.hpp"
#include "OpenGL.hpp"
#include "PlatformSpecific.hpp"
#include "Utils.hpp"

#include <SDL.h>

namespace eg::graphics_api::gl
{
#define GL_FUNC(name, proc) proc name;
#define GL_FUNC_OPT(name, proc) proc name;
#include "DesktopGLFunctions.inl"
#undef GL_FUNC
#undef GL_FUNC_OPT

static SDL_Window* glWindow;
static SDL_GLContext glContext;

static GLsync fences[MAX_CONCURRENT_FRAMES];

static void OpenGLMessageCallback(
	GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userData)
{
	if (glVendor == GLVendor::Nvidia && id == 131186) // Buffer performance warning
		return;
	if (glVendor == GLVendor::Intel && strstr(message, "used uninitialized"))
		return;

	LogLevel logLevel;
	switch (type)
	{
	case GL_DEBUG_SEVERITY_HIGH:
	case GL_DEBUG_TYPE_ERROR:
		logLevel = LogLevel::Error;
		break;
	case GL_DEBUG_SEVERITY_LOW:
	case GL_DEBUG_SEVERITY_MEDIUM:
		logLevel = LogLevel::Warning;
		break;
	default:
		logLevel = LogLevel::Info;
		break;
	}

	std::string_view messageView(message, static_cast<size_t>(length));

	// Some vendors include a newline at the end of the message. This removes the newline if present.
	if (messageView.back() == '\n')
	{
		messageView = messageView.substr(0, messageView.size() - 1);
	}

	Log(logLevel, "gl", "{0} {1}", id, messageView);

	if (severity == GL_DEBUG_SEVERITY_HIGH || type == GL_DEBUG_TYPE_ERROR)
	{
		EG_DEBUG_BREAK
		std::abort();
	}
}

namespace TextureStorageFallback
{
static void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
	GLenum firstTarget = target;
	GLenum lastTarget = target;
	if (target == GL_TEXTURE_CUBE_MAP)
	{
		firstTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
		lastTarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
	}

	for (GLenum t = firstTarget; t <= lastTarget; t++)
	{
		for (int level = 0; level < levels; level++)
		{
			glTexImage2D(
				t, level, internalformat, width >> level, height >> level, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			EG_ASSERT(glGetError() == GL_NO_ERROR);
		}
	}
}

static void glTexStorage2DMultisample(
	GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height,
	GLboolean fixedsamplelocations)
{
	glTexImage2DMultisample(target, samples, internalformat, width, height, fixedsamplelocations);
}

static void glTexStorage3D(
	GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
	for (int level = 0; level < levels; level++)
	{
		glTexImage3D(
			target, level, internalformat, width >> level, height >> level, depth, 0, GL_RGBA, GL_UNSIGNED_BYTE,
			nullptr);
	}
}

static void glTexStorage3DMultisample(
	GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth,
	GLboolean fixedsamplelocations)
{
	glTexImage3DMultisample(target, samples, internalformat, width, height, depth, fixedsamplelocations);
}
} // namespace TextureStorageFallback

bool InitializeGLPlatformSpecific(
	const GraphicsAPIInitArguments& initArguments, std::vector<const char*>& requiredExtensions)
{
	glContext = SDL_GL_CreateContext(initArguments.window);
	if (glContext == nullptr)
	{
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR, "Error Initializing OpenGL",
			"Could not create OpenGL context, make sure your graphics driver supports at least OpenGL 4.3.", nullptr);
		return false;
	}

	srgbBackBuffer = initArguments.defaultFramebufferSRGB;

#ifndef EG_GLES
	useGLESPath = initArguments.preferGLESPath;
#endif

	const char* missingFunction = nullptr;
#define GL_FUNC(name, proc)                                                                                            \
	if (!(::eg::graphics_api::gl::name = reinterpret_cast<proc>(SDL_GL_GetProcAddress(#name))))                        \
		missingFunction = #name;
#define GL_FUNC_OPT(name, proc) ::eg::graphics_api::gl::name = reinterpret_cast<proc>(SDL_GL_GetProcAddress(#name));
#include "DesktopGLFunctions.inl"
#undef GL_FUNC
#undef GL_FUNC_OPT

	if (missingFunction != nullptr)
	{
		std::string message = Concat({ "Missing OpenGL function ", missingFunction, "." });
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error Initializing OpenGL", message.c_str(), nullptr);
		return false;
	}

#ifndef EG_GLES
	if (glObjectLabel == nullptr)
	{
		glObjectLabel = [](GLenum, GLuint, GLsizei, const GLchar*) {};
	}
#endif

	glWindow = initArguments.window;

	requiredExtensions.push_back("GL_EXT_texture_filter_anisotropic");
	if (!useGLESPath)
	{
		requiredExtensions.push_back("GL_ARB_buffer_storage");
		requiredExtensions.push_back("GL_ARB_clear_texture");
	}
	if (initArguments.forceDepthZeroToOne)
	{
		requiredExtensions.push_back("GL_ARB_clip_control");
	}

	if (!IsExtensionSupported("GL_ARB_texture_storage"))
	{
		glTexStorage2D = TextureStorageFallback::glTexStorage2D;
		glTexStorage2DMultisample = TextureStorageFallback::glTexStorage2DMultisample;
		glTexStorage3D = TextureStorageFallback::glTexStorage3D;
		glTexStorage3DMultisample = TextureStorageFallback::glTexStorage3DMultisample;
	}

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

#ifndef EG_GLES
	if (initArguments.forceDepthZeroToOne)
	{
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
	}

	if (DevMode() && glDebugMessageCallback && glDebugMessageControl)
	{
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(OpenGLMessageCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
	}
#endif

	return true;
}

bool IsExtensionSupported(const char* name)
{
	return SDL_GL_ExtensionSupported(name);
}

void SetEnableVSync(bool enableVSync)
{
	if (!enableVSync)
		SDL_GL_SetSwapInterval(0);
	else if (SDL_GL_SetSwapInterval(-1) == -1)
		SDL_GL_SetSwapInterval(1);
}

void Shutdown()
{
	SDL_GL_DeleteContext(glContext);
}

void GetDrawableSize(int& width, int& height)
{
	SDL_GL_GetDrawableSize(glWindow, &width, &height);
}

void PlatformSpecificGetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
{
#ifdef EG_GLES
	deviceInfo.tessellation = false;
	deviceInfo.computeShader = false;
	deviceInfo.partialTextureViews = false;
#else
	for (int i = 0; i < 3; i++)
	{
		int ans;
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &ans);
		deviceInfo.maxComputeWorkGroupCount[i] = ans;
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &ans);
		deviceInfo.maxComputeWorkGroupSize[i] = ans;
	}
	deviceInfo.maxComputeWorkGroupInvocations = ToUnsigned(GetIntegerLimit(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS));
	deviceInfo.storageBufferOffsetAlignment = ToUnsigned(GetIntegerLimit(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT));
	deviceInfo.tessellation = true;
	deviceInfo.computeShader = true;
	deviceInfo.partialTextureViews = SDL_GL_ExtensionSupported("GL_ARB_texture_view");
#endif

	deviceInfo.maxClipDistances = ToUnsigned(GetIntegerLimit(GL_MAX_CLIP_DISTANCES));
	deviceInfo.maxTessellationPatchSize = ToUnsigned(GetIntegerLimit(GL_MAX_PATCH_VERTICES));
	deviceInfo.persistentMappedBuffers = true;
	deviceInfo.textureCubeMapArray = true;
	deviceInfo.blockTextureCompression = SDL_GL_ExtensionSupported("GL_EXT_texture_compression_s3tc") &&
	                                     SDL_GL_ExtensionSupported("GL_ARB_texture_compression_rgtc");
}

static GLsync loadFence;

void EndLoading()
{
	loadFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

bool IsLoadingComplete()
{
	GLenum status = glClientWaitSync(loadFence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
	if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED)
	{
		glDeleteSync(loadFence);
		return true;
	}
	return false;
}

void PlatformSpecificBeginFrame()
{
	if (fences[CFrameIdx()])
	{
		glClientWaitSync(fences[CFrameIdx()], 0, UINT64_MAX);
		glDeleteSync(fences[CFrameIdx()]);
	}
}

void PlatformSpecificEndFrame()
{
	fences[CFrameIdx()] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	glFlush();
	SDL_GL_SwapWindow(glWindow);
}
} // namespace eg::graphics_api::gl

#endif
