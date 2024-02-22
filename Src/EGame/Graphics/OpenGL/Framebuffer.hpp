#pragma once

#include "GL.hpp"

#include <optional>
#include <span>
#include <string_view>

#include "../Format.hpp"

namespace eg::graphics_api::gl
{
extern bool defaultFramebufferHasDepth;
extern bool defaultFramebufferHasStencil;

extern int drawableWidth;
extern int drawableHeight;

extern bool srgbBackBuffer;
extern bool hasWrittenToBackBuffer;

extern bool enableDefaultFramebufferSRGBEmulation;

struct FramebufferFormat
{
	std::span<const Format> colorAttachmentFormats;
	std::optional<Format> depthStencilAttachmentFormat;
	uint32_t sampleCount;

	static FramebufferFormat GetCurrent();

	size_t Hash() const;

	void PrintToStdout(std::string_view linePrefix, const FramebufferFormat* expected) const;
};

void AssertRenderPassActive(std::string_view opName);
void AssertRenderPassNotActive(std::string_view opName);

void AssertFramebufferComplete(GLenum target);

void BindCorrectFramebuffer();

void UpdateSRGBEmulationTexture(int width, int height);
void SRGBEmulationEndFrame();

void GLESAssertTextureBindNotInCurrentFramebuffer(const struct Texture& texture);
} // namespace eg::graphics_api::gl
