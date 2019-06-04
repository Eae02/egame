#include "OpenGL.hpp"
#include "Utils.hpp"
#include "OpenGLTexture.hpp"
#include "PipelineGraphics.hpp"
#include "../Graphics.hpp"
#include "../../Log.hpp"
#include "../../Alloc/ObjectPool.hpp"

#include <bitset>

#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#ifdef EG_WEB
#include <EGL/egl.h>
#else
#include <SDL.h>
#endif

namespace eg::graphics_api::gl
{
#ifdef EG_WEB
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
		IntelOpenSource
	};
	
	static GLVendor glVendor;
	
#ifndef EG_WEB
	static void OpenGLMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	                                  const GLchar* message, const void* userData)
	{
		if (glVendor == GLVendor::IntelOpenSource)
		{
			if (id == 17 || id == 14) //Clearing integer framebuffer attachments.
				return;
		}
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
			EG_DEBUG_BREAK
			std::abort();
		}
	}
#endif
	
#ifdef EG_WEB
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
	
	bool Initialize(const GraphicsAPIInitArguments& initArguments)
	{
#ifdef EG_WEB
		eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		
		eglInitialize(eglDisplay, nullptr, nullptr);
		
		EGLConfig eglConfig;
		int numEglConfigs;
		eglGetConfigs(eglDisplay, &eglConfig, 1, &numEglConfigs);
		
		eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, 0, nullptr);
		if (eglSurface == EGL_NO_SURFACE)
			return false;
		
		EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE, EGL_NONE };
		eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
		if (eglContext == EGL_NO_CONTEXT)
			return false;
		
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
		
		if (!initArguments.enableVSync)
			SDL_GL_SetSwapInterval(0);
		else if (SDL_GL_SetSwapInterval(-1) == -1)
			SDL_GL_SetSwapInterval(1);
		
		srgbBackBuffer = initArguments.defaultFramebufferSRGB;
		
		const char* requiredExtensions[] = 
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
				//SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error Initializing OpenGL", message.c_str(), nullptr);
				return false;
			}
		}
		
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
		
		std::string_view vendorName = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
		if (vendorName == "Intel Open Source Technology Center")
			glVendor = GLVendor::IntelOpenSource;
		else if (vendorName == "NVIDIA Corporation")
			glVendor = GLVendor::Nvidia;
		else
			glVendor = GLVendor::Unknown;
		
#ifndef EG_WEB
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(OpenGLMessageCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
#endif
		
		const char* rendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		Log(LogLevel::Info, "gl", "Using OpenGL renderer: '{0}'", rendererName);
		
		return true;
	}
	
	void GetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
	{
		auto GetIntegerLimit = [&] (GLenum name)
		{
			int res;
			glGetIntegerv(name, &res);
			return res;
		};
		
		deviceInfo.uniformBufferAlignment = (uint32_t)GetIntegerLimit(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
		deviceInfo.geometryShader = true;
		deviceInfo.concurrentResourceCreation = false;
		deviceInfo.depthRange = DepthRange::NegOneToOne;
		deviceInfo.timerTicksPerNS = 1.0f;
		
#ifdef EG_GLES
		deviceInfo.blockTextureCompression =
			Contains(supportedExtensions, "GL_EXT_texture_compression_s3tc") &&
			Contains(supportedExtensions, "GL_ARB_texture_compression_rgtc");
		deviceInfo.persistentMappedBuffers = false;
		deviceInfo.tessellation = false;
		deviceInfo.textureCubeMapArray = false;
		deviceInfo.maxTessellationPatchSize = 0;
		deviceInfo.maxMSAA = 1;
		deviceInfo.maxClipDistances = 0;
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
		
		deviceInfo.maxClipDistances = (uint32_t)GetIntegerLimit(GL_MAX_CLIP_DISTANCES);
		deviceInfo.maxMSAA = (uint32_t)GetIntegerLimit(GL_MAX_SAMPLES);
		deviceInfo.maxTessellationPatchSize = (uint32_t)GetIntegerLimit(GL_MAX_PATCH_VERTICES);
		deviceInfo.tessellation = true;
		deviceInfo.persistentMappedBuffers = true;
		deviceInfo.textureCubeMapArray = true;
		deviceInfo.blockTextureCompression =
			SDL_GL_ExtensionSupported("GL_EXT_texture_compression_s3tc") &&
			SDL_GL_ExtensionSupported("GL_ARB_texture_compression_rgtc");
#endif
	}
	
	void Shutdown()
	{
#ifdef EG_WEB
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
#ifdef EG_WEB
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
#ifdef EG_WEB
		eglQuerySurface(eglDisplay, eglSurface, EGL_WIDTH, &width);
		eglQuerySurface(eglDisplay, eglSurface, EGL_HEIGHT, &height);
#else
		SDL_GL_GetDrawableSize(glWindow, &width, &height);
#endif
	}
	
	void BeginFrame()
	{
		GetDrawableSize(drawableWidth, drawableHeight);
		
#ifndef EG_WEB
		if (fences[CFrameIdx()])
		{
			glClientWaitSync(fences[CFrameIdx()], GL_SYNC_FLUSH_COMMANDS_BIT, UINT64_MAX);
			glDeleteSync(fences[CFrameIdx()]);
		}
#endif
		
		viewportOutOfDate = true;
		scissorOutOfDate = true;
		hasWrittenToBackBuffer = false;
	}
	
	void EndFrame()
	{
#ifndef EG_WEB
		fences[CFrameIdx()] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		SDL_GL_SwapWindow(glWindow);
#endif
	}
	
	void DeviceWaitIdle()
	{
		glFinish();
	}
}
