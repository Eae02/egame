#include "OpenGL.hpp"
#include "Utils.hpp"
#include "OpenGLTexture.hpp"
#include "PipelineGraphics.hpp"
#include "../Graphics.hpp"
#include "../../Log.hpp"
#include "../../Alloc/ObjectPool.hpp"

#include <bitset>
#include <SDL.h>

namespace eg::graphics_api::gl
{
	static SDL_Window* glWindow;
	static SDL_GLContext glContext;
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
	
	bool Initialize(const GraphicsAPIInitArguments& initArguments)
	{
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
			"GL_ARB_direct_state_access",
			"GL_ARB_buffer_storage",
			"GL_ARB_clear_texture",
			"GL_EXT_texture_filter_anisotropic"
		};
		
		for (const char* ext : requiredExtensions)
		{
			if (!SDL_GL_ExtensionSupported(ext))
			{
				std::ostringstream messageStream;
				messageStream << "Required OpenGL extension " << ext << " is not supported by your graphics driver.";
				std::string message = messageStream.str();
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error Initializing OpenGL", message.c_str(), nullptr);
				return false;
			}
		}
		
		if (gl3wInit() != GL3W_OK)
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error Initializing OpenGL",
				"Unknown error occurred when initializing OpenGL (gl3wInit failed).", nullptr);
			return false;
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
		
		glWindow = initArguments.window;
		
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
		
		GLuint vao;
		glCreateVertexArrays(1, &vao);
		glBindVertexArray(vao);
		
		float maxAnistropyF;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnistropyF);
		maxAnistropy = (int)maxAnistropyF;
		
		std::string_view vendorName = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
		if (vendorName == "Intel Open Source Technology Center")
			glVendor = GLVendor::IntelOpenSource;
		else if (vendorName == "NVIDIA Corporation")
			glVendor = GLVendor::Nvidia;
		else
			glVendor = GLVendor::Unknown;
		
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(OpenGLMessageCallback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
		
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
		
		for (int i = 0; i < 3; i++)
		{
			int ans;
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &ans);
			deviceInfo.maxComputeWorkGroupCount[i] = ans;
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &ans);
			deviceInfo.maxComputeWorkGroupSize[i] = ans;
		}
		deviceInfo.maxComputeWorkGroupInvocations = GetIntegerLimit(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
		
		deviceInfo.uniformBufferAlignment = (uint32_t)GetIntegerLimit(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT);
		deviceInfo.maxTessellationPatchSize = (uint32_t)GetIntegerLimit(GL_MAX_PATCH_VERTICES);
		deviceInfo.maxClipDistances = (uint32_t)GetIntegerLimit(GL_MAX_CLIP_DISTANCES);
		deviceInfo.geometryShader = true;
		deviceInfo.tessellation = true;
		deviceInfo.textureCubeMapArray = true;
		deviceInfo.concurrentResourceCreation = false;
		deviceInfo.blockTextureCompression =
			SDL_GL_ExtensionSupported("GL_EXT_texture_compression_s3tc") &&
			SDL_GL_ExtensionSupported("GL_ARB_texture_compression_rgtc");
		deviceInfo.depthRange = DepthRange::NegOneToOne;
		deviceInfo.timerTicksPerNS = 1.0f;
	}
	
	void Shutdown()
	{
		SDL_GL_DeleteContext(glContext);
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
	
	void GetDrawableSize(int& width, int& height)
	{
		SDL_GL_GetDrawableSize(glWindow, &width, &height);
	}
	
	void BeginFrame()
	{
		SDL_GL_GetDrawableSize(glWindow, &drawableWidth, &drawableHeight);
		
		if (fences[CFrameIdx()])
		{
			glClientWaitSync(fences[CFrameIdx()], GL_SYNC_FLUSH_COMMANDS_BIT, UINT64_MAX);
			glDeleteSync(fences[CFrameIdx()]);
		}
		
		viewportOutOfDate = true;
		scissorOutOfDate = true;
		hasWrittenToBackBuffer = false;
	}
	
	void EndFrame()
	{
		fences[CFrameIdx()] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		SDL_GL_SwapWindow(glWindow);
	}
	
	void DeviceWaitIdle()
	{
		glFinish();
	}
}
