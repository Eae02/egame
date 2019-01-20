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
	
	extern int maxAnistropy;
	
	static bool defaultFramebufferHasDepth;
	static bool defaultFramebufferHasStencil;
	
	static int drawableWidth;
	static int drawableHeight;
	
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
			return false;
		
		if (!initArguments.enableVSync)
			SDL_GL_SetSwapInterval(0);
		else if (SDL_GL_SetSwapInterval(-1) == -1)
			SDL_GL_SetSwapInterval(1);
		
		if (gl3wInit() != GL3W_OK)
			return false;
		
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
		
		const char* rendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		Log(LogLevel::Info, "gfx", "Using OpenGL renderer: '{0}'", rendererName);
		
		return true;
	}
	
	void GetCapabilities(GraphicsCapabilities& capabilities)
	{
		int intRes;
		glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &intRes);
		capabilities.uniformBufferAlignment = intRes;
		
		capabilities.depthRange = DepthRange::NegOneToOne;
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
	
	extern bool viewportOutOfDate;
	extern bool scissorOutOfDate;
	
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
	}
	
	void EndFrame()
	{
		fences[CFrameIdx()] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		SDL_GL_SwapWindow(glWindow);
	}
	
	void InitScissorTest();
	
	void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
	{
		int numColorAttachments;
		bool hasDepth;
		bool hasStencil;
		if (beginInfo.framebuffer == nullptr)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			numColorAttachments = 1;
			hasDepth = defaultFramebufferHasDepth;
			hasStencil = defaultFramebufferHasStencil;
			
			SetViewport(cc, 0, 0, drawableWidth, drawableHeight);
			SetScissor(cc, 0, 0, drawableWidth, drawableHeight);
		}
		else
		{
			EG_UNREACHABLE
		}
		
		SetEnabled<GL_SCISSOR_TEST>(false);
		
		uint32_t numInvalidateAttachments = 0;
		GLenum invalidateAttachments[MAX_COLOR_ATTACHMENTS + 2];
		
		if (hasDepth)
		{
			if (beginInfo.depthLoadOp == beginInfo.stencilLoadOp && hasStencil)
			{
				if (beginInfo.depthLoadOp == AttachmentLoadOp::Clear)
				{
					glClearBufferfi(GL_DEPTH_STENCIL, 0, beginInfo.depthClearValue, beginInfo.stencilClearValue);
				}
				else if (beginInfo.depthLoadOp == AttachmentLoadOp::Discard)
				{
					if (beginInfo.framebuffer == nullptr)
					{
						invalidateAttachments[numInvalidateAttachments++] = GL_DEPTH;
						invalidateAttachments[numInvalidateAttachments++] = GL_STENCIL;
					}
					else
					{
						invalidateAttachments[numInvalidateAttachments++] = GL_DEPTH_STENCIL_ATTACHMENT;
					}
				}
			}
			else
			{
				if (beginInfo.depthLoadOp == AttachmentLoadOp::Clear)
				{
					glClearBufferfv(GL_DEPTH, 0, &beginInfo.depthClearValue);
				}
				else if (beginInfo.depthLoadOp == AttachmentLoadOp::Discard)
				{
					if (beginInfo.framebuffer == nullptr)
					{
						invalidateAttachments[numInvalidateAttachments++] = GL_DEPTH;
					}
					else
					{
						invalidateAttachments[numInvalidateAttachments++] = GL_DEPTH_ATTACHMENT;
					}
				}
				
				if (hasStencil)
				{
					if (beginInfo.stencilLoadOp == AttachmentLoadOp::Clear)
					{
						GLuint value = beginInfo.stencilClearValue;
						glClearBufferuiv(GL_STENCIL, 0, &value);
					}
					else if (beginInfo.stencilLoadOp == AttachmentLoadOp::Discard)
					{
						if (beginInfo.framebuffer == nullptr)
						{
							invalidateAttachments[numInvalidateAttachments++] = GL_STENCIL;
						}
						else
						{
							invalidateAttachments[numInvalidateAttachments++] = GL_STENCIL_ATTACHMENT;
						}
					}
				}
			}
		}
		
		for (int i = 0; i < numColorAttachments; i++)
		{
			const RenderPassColorAttachment& attachment = beginInfo.colorAttachments[i];
			if (attachment.loadOp == AttachmentLoadOp::Clear)
			{
				glClearBufferfv(GL_COLOR, i, &attachment.clearValue.r);
			}
			else if (attachment.loadOp == AttachmentLoadOp::Discard)
			{
				if (beginInfo.framebuffer == nullptr)
				{
					invalidateAttachments[numInvalidateAttachments++] = GL_COLOR;
				}
				else
				{
					invalidateAttachments[numInvalidateAttachments++] = (GLenum)(GL_COLOR_ATTACHMENT0 + i);
				}
			}
		}
		
		if (numInvalidateAttachments != 0)
		{
			glInvalidateFramebuffer(GL_FRAMEBUFFER, numInvalidateAttachments, invalidateAttachments);
		}
		
		InitScissorTest();
	}
	
	void EndRenderPass(CommandContextHandle cc) { }
}
