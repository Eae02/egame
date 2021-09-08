#include "OpenGL.hpp"
#include "Utils.hpp"
#include "OpenGLTexture.hpp"
#include "PipelineGraphics.hpp"
#include "../Graphics.hpp"
#include "../../Log.hpp"
#include "../../Alloc/ObjectPool.hpp"

#include <sstream>
#include <bitset>

#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#ifdef __EMSCRIPTEN__
#include <EGL/egl.h>
#else
#include <SDL.h>
#endif

namespace eg::graphics_api::gl
{
#ifdef __EMSCRIPTEN__
	static EGLDisplay eglDisplay;
	static EGLSurface eglSurface;
	static EGLContext eglContext;
#else
	static SDL_Window* glWindow;
	static SDL_GLContext glContext;
#endif
	
	static GLsync fences[MAX_CONCURRENT_FRAMES];
	
	extern int maxAnistropy;
	
	extern bool defaultFramebufferHasDepth;
	extern bool defaultFramebufferHasStencil;
	
	extern int drawableWidth;
	extern int drawableHeight;
	
	extern bool srgbBackBuffer;
	extern bool hasWrittenToBackBuffer;
	
	enum class GLVendor
	{
		Unknown,
		Nvidia,
		Intel
	};
	
	static std::string rendererName;
	static std::string vendorName;
	static GLVendor glVendor;
	
#ifndef __EMSCRIPTEN__
	static void OpenGLMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	                                  const GLchar* message, const void* userData)
	{
		if (glVendor == GLVendor::Nvidia)
		{
			if (id == 131186) //Buffer performance warning
				return;
		}
		
		LogLevel logLevel;
		if (severity == GL_DEBUG_SEVERITY_HIGH || type == GL_DEBUG_TYPE_ERROR)
		{
			logLevel = LogLevel::Error;
		}
		else if (severity == GL_DEBUG_SEVERITY_LOW || severity == GL_DEBUG_SEVERITY_MEDIUM)
		{
			logLevel = LogLevel::Warning;
		}
		else
		{
			logLevel = LogLevel::Info;
		}
		
		std::string_view messageView(message, static_cast<size_t>(length));
		
		//Some vendors include a newline at the end of the message. This removes the newline if present.
		if (messageView.back() == '\n')
		{
			messageView = messageView.substr(0, messageView.size() - 1);
		}
		
		Log(logLevel, "gl", "{0} {1}", id, messageView);
		
		if (severity == GL_DEBUG_SEVERITY_HIGH || type == GL_DEBUG_TYPE_ERROR)
		{
			if (glVendor == GLVendor::Intel && strstr(message, "used uninitialized"))
				return;
			
			EG_DEBUG_BREAK
			std::abort();
		}
	}
#endif
	
#ifdef __EMSCRIPTEN__
	static std::vector<std::string_view> supportedExtensions;
	
	bool IsExtensionSupported(const char* name)
	{
		return Contains(supportedExtensions, name);
	}
#else
	bool IsExtensionSupported(const char* name)
	{
		return SDL_GL_ExtensionSupported(name);
	}
#endif
	
	static DepthRange depthRange = DepthRange::NegOneToOne;
	
	std::span<std::string> GetDeviceNames()
	{
		return { &rendererName, 1 };
	}
	
	bool Initialize(const GraphicsAPIInitArguments& initArguments)
	{
#ifdef __EMSCRIPTEN__
		if (initArguments.forceDepthZeroToOne)
			return false;
		
		eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		
		eglInitialize(eglDisplay, nullptr, nullptr);
		
		EGLConfig eglConfig;
		int numEglConfigs;
		eglGetConfigs(eglDisplay, &eglConfig, 1, &numEglConfigs);
		
		std::vector<EGLint> surfaceAttribs = { EGL_CONTEXT_CLIENT_VERSION, 3 };
		if (initArguments.defaultFramebufferSRGB)
		{
			surfaceAttribs.push_back(EGL_COLORSPACE);
			surfaceAttribs.push_back(EGL_COLORSPACE_sRGB);
		}
		surfaceAttribs.push_back(EGL_NONE);
		surfaceAttribs.push_back(EGL_NONE);
		
		eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, 0, surfaceAttribs.data());
		if (eglSurface == EGL_NO_SURFACE)
		{
			std::cout << "eglCreateWindowSurface failed: " << std::hex << eglGetError() << std::endl;
			return false;
		}
		
		std::vector<EGLint> contextAttribs = { EGL_CONTEXT_CLIENT_VERSION, 3 };
		contextAttribs.push_back(EGL_NONE);
		contextAttribs.push_back(EGL_NONE);
		
		eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs.data());
		if (eglContext == EGL_NO_CONTEXT)
		{
			std::cout << "eglCreateContext failed: " << std::hex << eglGetError() << std::endl;
			return false;
		}
		
		if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext))
			return false;
		
		const char* requiredExtensions[] = 
		{
			"GL_EXT_texture_filter_anisotropic"
		};
		
		SplitString(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)), ' ', supportedExtensions);
#else
		glContext = SDL_GL_CreateContext(initArguments.window);
		if (glContext == nullptr)
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error Initializing OpenGL",
				"Could not create OpenGL context, make sure your graphics driver supports at least OpenGL 4.3.", nullptr);
			return false;
		}
		
		srgbBackBuffer = initArguments.defaultFramebufferSRGB;
		
		std::vector<const char*> requiredExtensions = 
		{
			"GL_ARB_buffer_storage",
			"GL_ARB_clear_texture",
			"GL_EXT_texture_filter_anisotropic"
		};
		
		if (gl3wInit() != GL3W_OK)
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error Initializing OpenGL",
				"Unknown error occurred when initializing OpenGL (gl3wInit failed).", nullptr);
			return false;
		}
		
		glWindow = initArguments.window;
		
		if (initArguments.forceDepthZeroToOne)
		{
			requiredExtensions.push_back("GL_ARB_clip_control");
		}
		
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif
		
		for (const char* ext : requiredExtensions)
		{
			if (!IsExtensionSupported(ext))
			{
				std::ostringstream messageStream;
				messageStream << "Required OpenGL extension " << ext << " is not supported by your graphics driver.";
				std::string message = messageStream.str();
				std::cout << message << std::endl;
#ifndef __EMSCRIPTEN__
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error Initializing OpenGL", message.c_str(), nullptr);
#endif
				return false;
			}
		}
		
#ifndef __EMSCRIPTEN__
		if (initArguments.forceDepthZeroToOne)
		{
			glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
			depthRange = DepthRange::ZeroToOne;
		}
#endif
		
		if (initArguments.defaultDepthStencilFormat == Format::Depth32 ||
		    initArguments.defaultDepthStencilFormat == Format::Depth16)
		{
			defaultFramebufferHasDepth = true;
			defaultFramebufferHasStencil = false;
		}
		
		if (initArguments.defaultDepthStencilFormat == Format::Depth24Stencil8 ||
		    initArguments.defaultDepthStencilFormat == Format::Depth32Stencil8)
		{
			defaultFramebufferHasDepth = true;
			defaultFramebufferHasStencil = true;
		}
		
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		
		GLuint vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		
		float maxAnistropyF;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnistropyF);
		maxAnistropy = (int)maxAnistropyF;
		
		vendorName = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
		if (strstr(vendorName.c_str(), "Intel"))
			glVendor = GLVendor::Intel;
		else if (strstr(vendorName.c_str(), "NVIDIA"))
			glVendor = GLVendor::Nvidia;
		else
			glVendor = GLVendor::Unknown;
		
		rendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		
#ifndef __EMSCRIPTEN__
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(OpenGLMessageCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
#endif
		
		Log(LogLevel::Info, "gl", "Using OpenGL renderer: '{0}', by vendor: '{1}'", rendererName, vendorName);
		
		return true;
	}
	
	void SetEnableVSync(bool enableVSync)
	{
#ifndef __EMSCRIPTEN__
		if (!enableVSync)
			SDL_GL_SetSwapInterval(0);
		else if (SDL_GL_SetSwapInterval(-1) == -1)
			SDL_GL_SetSwapInterval(1);
#endif
	}
	
	void GetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
	{
		auto GetIntegerLimit = [&] (GLenum name)
		{
			int res;
			glGetIntegerv(name, &res);
			return res;
		};
		
		deviceInfo.uniformBufferOffsetAlignment = (uint32_t)GetIntegerLimit(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
		deviceInfo.geometryShader               = true;
		deviceInfo.concurrentResourceCreation   = false;
		deviceInfo.depthRange                   = depthRange;
		deviceInfo.timerTicksPerNS              = 1.0f;
		deviceInfo.deviceName                   = rendererName;
		deviceInfo.deviceVendorName             = vendorName;
		deviceInfo.maxMSAA                      = (uint32_t)GetIntegerLimit(GL_MAX_SAMPLES);
		
#ifdef EG_GLES
		deviceInfo.blockTextureCompression =
			Contains(supportedExtensions, "GL_EXT_texture_compression_s3tc") &&
			Contains(supportedExtensions, "GL_ARB_texture_compression_rgtc");
		deviceInfo.persistentMappedBuffers  = false;
		deviceInfo.tessellation             = false;
		deviceInfo.textureCubeMapArray      = false;
		deviceInfo.maxTessellationPatchSize = 0;
		deviceInfo.maxClipDistances         = 0;
		deviceInfo.computeShader            = false;
#else
		for (int i = 0; i < 3; i++)
		{
			int ans;
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &ans);
			deviceInfo.maxComputeWorkGroupCount[i] = ans;
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &ans);
			deviceInfo.maxComputeWorkGroupSize[i] = ans;
		}
		deviceInfo.maxComputeWorkGroupInvocations = GetIntegerLimit(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
		deviceInfo.storageBufferOffsetAlignment = (uint32_t)GetIntegerLimit(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT);
		
		deviceInfo.maxClipDistances         = (uint32_t)GetIntegerLimit(GL_MAX_CLIP_DISTANCES);
		deviceInfo.maxTessellationPatchSize = (uint32_t)GetIntegerLimit(GL_MAX_PATCH_VERTICES);
		deviceInfo.tessellation             = true;
		deviceInfo.computeShader            = true;
		deviceInfo.persistentMappedBuffers  = true;
		deviceInfo.textureCubeMapArray      = true;
		deviceInfo.blockTextureCompression  =
			SDL_GL_ExtensionSupported("GL_EXT_texture_compression_s3tc") &&
			SDL_GL_ExtensionSupported("GL_ARB_texture_compression_rgtc");
#endif
	}
	
	void Shutdown()
	{
#ifdef __EMSCRIPTEN__
		eglDestroyContext(eglDisplay, eglContext);
		eglDestroySurface(eglDisplay, eglSurface);
#else
		SDL_GL_DeleteContext(glContext);
#endif
	}
	
	static GLsync loadFence;
	
	void EndLoading()
	{
		loadFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	}
	
	bool IsLoadingComplete()
	{
#ifdef __EMSCRIPTEN__
		return true;
#else
		GLenum status = glClientWaitSync(loadFence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
		if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED)
		{
			glDeleteSync(loadFence);
			return true;
		}
		return false;
#endif
	}
	
	void GetDrawableSize(int& width, int& height)
	{
#ifdef __EMSCRIPTEN__
		eglQuerySurface(eglDisplay, eglSurface, EGL_WIDTH, &width);
		eglQuerySurface(eglDisplay, eglSurface, EGL_HEIGHT, &height);
#else
		SDL_GL_GetDrawableSize(glWindow, &width, &height);
#endif
	}
	
	void BeginFrame()
	{
		GetDrawableSize(drawableWidth, drawableHeight);
		
#ifndef __EMSCRIPTEN__
		if (fences[CFrameIdx()])
		{
			glClientWaitSync(fences[CFrameIdx()], 0, UINT64_MAX);
			glDeleteSync(fences[CFrameIdx()]);
		}
#endif
		
		viewportOutOfDate = true;
		scissorOutOfDate = true;
		hasWrittenToBackBuffer = false;
	}
	
	void EndFrame()
	{
#ifndef __EMSCRIPTEN__
		fences[CFrameIdx()] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		glFlush();
		SDL_GL_SwapWindow(glWindow);
#endif
	}
	
	void DeviceWaitIdle()
	{
		glFinish();
	}
	
	void DebugLabelBegin(CommandContextHandle, const char* label, const float* color)
	{
#ifndef __EMSCRIPTEN__
		glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label);
#endif
	}
	
	void DebugLabelEnd(CommandContextHandle)
	{
#ifndef __EMSCRIPTEN__
		glPopDebugGroup();
#endif
	}
	
	void DebugLabelInsert(CommandContextHandle, const char* label, const float* color)
	{
#ifndef __EMSCRIPTEN__
		glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, label);
#endif
	}
}
