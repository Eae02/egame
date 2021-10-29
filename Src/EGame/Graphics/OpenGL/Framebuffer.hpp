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
	
	void AssertFramebufferComplete(GLenum target);
	
	void BindCorrectFramebuffer();
}
