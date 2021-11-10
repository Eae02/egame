#pragma once

#include "GL.hpp"

namespace eg::graphics_api::gl
{
	extern bool defaultFramebufferHasDepth;
	extern bool defaultFramebufferHasStencil;
	
	extern int drawableWidth;
	extern int drawableHeight;
	
	extern bool srgbBackBuffer;
	extern bool hasWrittenToBackBuffer;
	
	extern bool enableDefaultFramebufferSRGBEmulation;
	
	void AssertRenderPassActive(std::string_view opName);
	void AssertRenderPassNotActive(std::string_view opName);
	
	void AssertFramebufferComplete(GLenum target);
	
	void BindCorrectFramebuffer();
	
	void UpdateSRGBEmulationTexture(int width, int height);
	void SRGBEmulationEndFrame();
	
	void GLESAssertTextureBindNotInCurrentFramebuffer(const struct Texture& texture);
}
