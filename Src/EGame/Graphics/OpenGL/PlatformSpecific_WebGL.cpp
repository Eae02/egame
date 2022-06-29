#ifdef __EMSCRIPTEN__

#include "OpenGL.hpp"
#include "Utils.hpp"
#include "Framebuffer.hpp"

#include <EGL/egl.h>

namespace eg::graphics_api::gl
{
	static EGLDisplay eglDisplay;
	static EGLSurface eglSurface;
	static EGLContext eglContext;
	
	static std::vector<std::string_view> supportedExtensions;
	
	bool InitializeGLPlatformSpecific(const GraphicsAPIInitArguments& initArguments, std::vector<const char*>& requiredExtensions)
	{
		if (initArguments.forceDepthZeroToOne)
		{
			std::cout << "initArguments.forceDepthZeroToOne was true, but this is not supported in WebGL" << std::endl;
			return false;
		}
		
		eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		
		eglInitialize(eglDisplay, nullptr, nullptr);
		
		EGLConfig eglConfig;
		int numEglConfigs;
		eglGetConfigs(eglDisplay, &eglConfig, 1, &numEglConfigs);
		
		EGLint surfaceAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE, EGL_NONE };
		eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, 0, surfaceAttribs);
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
		
		requiredExtensions.push_back("GL_EXT_texture_filter_anisotropic");
		
		SplitString(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)), ' ', supportedExtensions);
		
		glesFormatSupport.floatColorBuffer     = IsExtensionSupported("GL_EXT_color_buffer_float");
		glesFormatSupport.floatLinearFiltering = IsExtensionSupported("GL_OES_texture_float_linear");
		glesFormatSupport.floatBlend           = IsExtensionSupported("GL_EXT_float_blend");
		glesFormatSupport.compressedS3TC       = IsExtensionSupported("GL_WEBGL_compressed_texture_s3tc");
		glesFormatSupport.compressedS3TCSRGB   = IsExtensionSupported("GL_WEBGL_compressed_texture_s3tc_srgb");
		
		enableDefaultFramebufferSRGBEmulation = initArguments.defaultFramebufferSRGB;
		
		return true;
	}
	
	bool IsExtensionSupported(const char* name)
	{
		return Contains(supportedExtensions, name);
	}
	
	void SetEnableVSync(bool enableVSync) { }
	
	void Shutdown()
	{
		eglDestroyContext(eglDisplay, eglContext);
		eglDestroySurface(eglDisplay, eglSurface);
	}
	
	void GetDrawableSize(int& width, int& height)
	{
		eglQuerySurface(eglDisplay, eglSurface, EGL_WIDTH, &width);
		eglQuerySurface(eglDisplay, eglSurface, EGL_HEIGHT, &height);
	}
	
	void PlatformSpecificGetDeviceInfo(GraphicsDeviceInfo& deviceInfo)
	{
		deviceInfo.blockTextureCompression =
			Contains(supportedExtensions, "GL_EXT_texture_compression_s3tc") &&
			Contains(supportedExtensions, "GL_ARB_texture_compression_rgtc");
		deviceInfo.persistentMappedBuffers  = false;
		deviceInfo.tessellation             = false;
		deviceInfo.textureCubeMapArray      = false;
		deviceInfo.maxTessellationPatchSize = 0;
		deviceInfo.maxClipDistances         = 0;
		deviceInfo.computeShader            = false;
	}
	
	void EndLoading() { }
	
	bool IsLoadingComplete() { return true; }
	
	void PlatformSpecificBeginFrame() { }
	
	void PlatformSpecificEndFrame() { }
}

#endif
