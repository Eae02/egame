#include "OpenGL.hpp"
#include "Utils.hpp"
#include "../Graphics.hpp"
#include "../../Log.hpp"
#include "../../Alloc/ObjectPool.hpp"

#include <SDL2/SDL.h>
#include <GL/gl3w.h>

namespace eg::graphics_api::gl
{
	static SDL_Window* glWindow;
	static SDL_GLContext glContext;
	static GLsync fences[MAX_CONCURRENT_FRAMES];
	
	extern bool supportsSpirV;
	extern int maxAnistropy;
	
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
	
	bool Initialize(SDL_Window* window)
	{
		glContext = SDL_GL_CreateContext(window);
		if (glContext == nullptr)
			return false;
		
		if (gl3wInit() != GL3W_OK)
			return false;
		
		glWindow = window;
		
		supportsSpirV = SDL_GL_ExtensionSupported("GL_ARB_gl_spirv");
		
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		
		GLuint vao;
		glCreateVertexArrays(1, &vao);
		glBindVertexArray(vao);
		
		float maxAnistropyF;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnistropyF);
		maxAnistropy = maxAnistropyF;
		
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
		
		return true;
	}
	
	void Shutdown()
	{
		SDL_GL_DeleteContext(glContext);
	}
	
	std::tuple<int, int> GetDrawableSize()
	{
		int w, h;
		SDL_GL_GetDrawableSize(glWindow, &w, &h);
		return std::make_tuple(w, h);
	}
	
	void BeginFrame()
	{
		if (fences[CFrameIdx()])
		{
			glClientWaitSync(fences[CFrameIdx()], GL_SYNC_FLUSH_COMMANDS_BIT, UINT64_MAX);
			glDeleteSync(fences[CFrameIdx()]);
		}
	}
	
	void EndFrame()
	{
		fences[CFrameIdx()] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		SDL_GL_SwapWindow(glWindow);
	}
	
	void SetViewport(CommandContextHandle, int x, int y, int w, int h)
	{
		glViewport(x, y, w, h);
	}
	
	void SetScissor(CommandContextHandle, int x, int y, int w, int h)
	{
		glScissor(x, y, w, h);
	}
	
	void InitScissorTest();
	
	void ClearFBColor(CommandContextHandle, int buffer, const Color& color)
	{
		SetEnabled<GL_SCISSOR_TEST>(false);
		glClearBufferfv(GL_COLOR, buffer, &color.r);
		InitScissorTest();
	}
	
	void ClearFBDepth(CommandContextHandle, float depth)
	{
		SetEnabled<GL_SCISSOR_TEST>(false);
		glClearBufferfv(GL_DEPTH, 0, &depth);
		InitScissorTest();
	}
	
	void ClearFBStencil(CommandContextHandle, uint32_t value)
	{
		SetEnabled<GL_SCISSOR_TEST>(false);
		glClearBufferuiv(GL_STENCIL, 0, &value);
		InitScissorTest();
	}
}
