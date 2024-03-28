#include "Framebuffer.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "../../String.hpp"
#include "OpenGL.hpp"
#include "OpenGLTexture.hpp"
#include "Pipeline.hpp"
#include "PipelineGraphics.hpp"
#include "Utils.hpp"

namespace eg::graphics_api::gl
{
bool defaultFramebufferHasDepth;
bool defaultFramebufferHasStencil;

int drawableWidth;
int drawableHeight;

bool srgbBackBuffer;
bool hasWrittenToBackBuffer;

bool enableDefaultFramebufferSRGBEmulation = false;
GLuint defaultFramebuffer = 0;

static bool isInsideRenderPass = false;

void AssertRenderPassActive(std::string_view opName)
{
	if (!isInsideRenderPass)
	{
		EG_PANIC(
			"Attempted to run " << opName
								<< " outside a render pass. This operation must be run inside a render pass.");
	}
}

void AssertRenderPassNotActive(std::string_view opName)
{
	if (isInsideRenderPass)
	{
		EG_PANIC(
			"Attempted to run " << opName
								<< " inside a render pass. This operation must be run outside a render pass.");
	}
}

struct ResolveFBO
{
	GLenum mask;
	GLuint framebuffers[2];
};

struct Framebuffer
{
	GLuint framebuffer;
	uint32_t numColorAttachments;
	uint32_t sampleCount;
	Format colorAttachmentFormats[MAX_COLOR_ATTACHMENTS];
	std::optional<Format> depthStencilAttachmentFormat;

	bool hasSRGB;
	bool hasDepth;
	bool hasStencil;
	uint32_t width;
	uint32_t height;
	std::vector<ResolveFBO> resolveFBOs;
	std::vector<Texture*> attachments;
};

static ObjectPool<Framebuffer> framebuffers;

inline Framebuffer* UnwrapFramebuffer(FramebufferHandle handle)
{
	return reinterpret_cast<Framebuffer*>(handle);
}

void AssertFramebufferComplete(GLenum target)
{
	switch (glCheckFramebufferStatus(target))
	{
#define FRAMEBUFFER_STATUS_CASE(id)                                                                                    \
	case id: EG_PANIC("Incomplete framebuffer: " #id);
		FRAMEBUFFER_STATUS_CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
		FRAMEBUFFER_STATUS_CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
		FRAMEBUFFER_STATUS_CASE(GL_FRAMEBUFFER_UNSUPPORTED)
		FRAMEBUFFER_STATUS_CASE(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
		FRAMEBUFFER_STATUS_CASE(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)
	default: break;
	}
}

static void AttachTexture(GLenum target, Framebuffer& framebuffer, GLuint fbo, const FramebufferAttachment& attachment)
{
	Texture* texture = UnwrapTexture(attachment.texture);

	TextureSubresourceLayers resolvedSubresource = attachment.subresource.ResolveRem(texture->arrayLayers);

	framebuffer.attachments.push_back(texture);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);

	if ((texture->type == GL_TEXTURE_2D_ARRAY || texture->type == GL_TEXTURE_CUBE_MAP_ARRAY ||
	     texture->type == GL_TEXTURE_CUBE_MAP) &&
	    resolvedSubresource.numArrayLayers == 1)
	{
		if (useGLESPath && texture->type == GL_TEXTURE_CUBE_MAP)
		{
			glFramebufferTexture2D(
				GL_READ_FRAMEBUFFER, target, GL_TEXTURE_CUBE_MAP_POSITIVE_X + resolvedSubresource.firstArrayLayer,
				texture->texture, resolvedSubresource.mipLevel);
			return;
		}
		else
		{
			glFramebufferTextureLayer(
				GL_READ_FRAMEBUFFER, target, texture->texture, resolvedSubresource.mipLevel,
				resolvedSubresource.firstArrayLayer);
		}
	}
	else
	{
		if (useGLESPath)
		{
			glFramebufferTexture2D(
				GL_READ_FRAMEBUFFER, target, GL_TEXTURE_2D, texture->texture, resolvedSubresource.mipLevel);
		}
		else
		{
			glFramebufferTexture(GL_READ_FRAMEBUFFER, target, texture->texture, resolvedSubresource.mipLevel);
		}
	}
}

FramebufferHandle CreateFramebuffer(const FramebufferCreateInfo& createInfo)
{
	Framebuffer* framebuffer = framebuffers.New();
	glGenFramebuffers(1, &framebuffer->framebuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);

	if (createInfo.label != nullptr)
	{
		glObjectLabel(GL_FRAMEBUFFER, framebuffer->framebuffer, -1, createInfo.label);
	}

	framebuffer->numColorAttachments = UnsignedNarrow<uint32_t>(createInfo.colorAttachments.size());
	framebuffer->hasDepth = false;
	framebuffer->hasStencil = false;
	framebuffer->hasSRGB = false;
	framebuffer->sampleCount = 0;

	bool hasSetSize = false;
	auto SetSize = [&](uint32_t w, uint32_t h)
	{
		if (!hasSetSize)
		{
			framebuffer->width = w;
			framebuffer->height = h;
		}
		else if (framebuffer->width != w || framebuffer->height != h)
		{
			EG_PANIC("Inconsistent framebuffer attachment resolution");
		}
	};

	GLenum drawBuffers[MAX_COLOR_ATTACHMENTS];

	auto Attach = [&](GLenum target, const FramebufferAttachment& attachment, Format* formatOut)
	{
		Texture* texture = UnwrapTexture(attachment.texture);

		if (framebuffer->sampleCount == 0)
			framebuffer->sampleCount = texture->sampleCount;
		else if (framebuffer->sampleCount != texture->sampleCount)
			EG_PANIC("Framebuffer attachment sample count mismatch");

		if (formatOut != nullptr)
			*formatOut = texture->format;

		uint32_t realWidth = texture->width >> attachment.subresource.mipLevel;
		uint32_t realHeight = texture->height >> attachment.subresource.mipLevel;
		SetSize(realWidth, realHeight);
		if (IsSRGBFormat(texture->format))
			framebuffer->hasSRGB = true;

		AttachTexture(target, *framebuffer, framebuffer->framebuffer, attachment);
	};

	for (uint32_t i = 0; i < createInfo.colorAttachments.size(); i++)
	{
		Attach(GL_COLOR_ATTACHMENT0 + i, createInfo.colorAttachments[i], &framebuffer->colorAttachmentFormats[i]);
		drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
	}

	if (createInfo.depthStencilAttachment.texture != nullptr)
	{
		Format format = UnwrapTexture(createInfo.depthStencilAttachment.texture)->format;
		framebuffer->depthStencilAttachmentFormat = format;

		if (format == Format::Depth32 || format == Format::Depth16)
		{
			Attach(GL_DEPTH_ATTACHMENT, createInfo.depthStencilAttachment, nullptr);
		}
		else if (format == Format::Depth24Stencil8 || format == Format::Depth32Stencil8)
		{
			Attach(GL_DEPTH_STENCIL_ATTACHMENT, createInfo.depthStencilAttachment, nullptr);
		}
		else
		{
			EG_PANIC("Invalid depth stencil attachment format");
		}
		framebuffer->hasDepth = true;
	}

	if (!createInfo.colorAttachments.empty())
	{
		glDrawBuffers(ToInt(createInfo.colorAttachments.size()), drawBuffers);
	}

	AssertFramebufferComplete(GL_FRAMEBUFFER);

	for (size_t i = 0; i < createInfo.colorResolveAttachments.size(); i++)
	{
		if (createInfo.colorResolveAttachments[i].texture == nullptr)
			continue;

		auto& fboPair = framebuffer->resolveFBOs.emplace_back();
		fboPair.mask = GL_COLOR_BUFFER_BIT;
		glGenFramebuffers(2, fboPair.framebuffers);
		AttachTexture(GL_COLOR_ATTACHMENT0, *framebuffer, fboPair.framebuffers[0], createInfo.colorAttachments[i]);
		AttachTexture(
			GL_COLOR_ATTACHMENT0, *framebuffer, fboPair.framebuffers[1], createInfo.colorResolveAttachments[i]);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		AssertFramebufferComplete(GL_READ_FRAMEBUFFER);
	}

	if (createInfo.depthStencilResolveAttachment.texture != nullptr)
	{
		auto& fboPair = framebuffer->resolveFBOs.emplace_back();
		fboPair.mask = GL_DEPTH_BUFFER_BIT;
		glGenFramebuffers(2, fboPair.framebuffers);
		AttachTexture(GL_DEPTH_ATTACHMENT, *framebuffer, fboPair.framebuffers[0], createInfo.depthStencilAttachment);
		AttachTexture(
			GL_DEPTH_ATTACHMENT, *framebuffer, fboPair.framebuffers[1], createInfo.depthStencilResolveAttachment);
		AssertFramebufferComplete(GL_READ_FRAMEBUFFER);
	}

	return reinterpret_cast<FramebufferHandle>(framebuffer);
}

void DestroyFramebuffer(FramebufferHandle handle)
{
	Framebuffer* framebuffer = UnwrapFramebuffer(handle);
	glDeleteFramebuffers(1, &framebuffer->framebuffer);
	for (const ResolveFBO& resolveFbo : framebuffer->resolveFBOs)
		glDeleteFramebuffers(2, resolveFbo.framebuffers);
	framebuffers.Delete(framebuffer);
}

Framebuffer* currentFramebuffer = nullptr;

void BindCorrectFramebuffer()
{
	uint32_t fbWidth;
	uint32_t fbHeight;

	if (currentFramebuffer != nullptr)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, currentFramebuffer->framebuffer);
		fbWidth = currentFramebuffer->width;
		fbHeight = currentFramebuffer->height;
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
		fbWidth = drawableWidth;
		fbHeight = drawableHeight;
	}

	SetViewport(nullptr, 0, 0, static_cast<float>(fbWidth), static_cast<float>(fbHeight));
	SetScissor(nullptr, 0, 0, fbWidth, fbHeight);
}

void BeginRenderPass(CommandContextHandle cc, const RenderPassBeginInfo& beginInfo)
{
	AssertRenderPassNotActive("BeginRenderPass");
	isInsideRenderPass = true;

	currentFramebuffer = UnwrapFramebuffer(beginInfo.framebuffer);
	BindCorrectFramebuffer();

	uint32_t numColorAttachments;
	bool hasDepth;
	bool hasStencil;
	bool forceClear;
	if (beginInfo.framebuffer == nullptr)
	{
		numColorAttachments = 1;
		hasDepth = defaultFramebufferHasDepth;
		hasStencil = defaultFramebufferHasStencil;
		forceClear = !hasWrittenToBackBuffer;

#ifndef __EMSCRIPTEN__
		SetEnabled<GL_FRAMEBUFFER_SRGB>(srgbBackBuffer);
#endif
	}
	else
	{
		numColorAttachments = currentFramebuffer->numColorAttachments;
		hasDepth = currentFramebuffer->hasDepth;
		hasStencil = currentFramebuffer->hasStencil;
		forceClear = false;

#ifndef __EMSCRIPTEN__
		SetEnabled<GL_FRAMEBUFFER_SRGB>(true);
#endif
	}

	SetEnabled<GL_SCISSOR_TEST>(false);
	if (!IsDepthWriteEnabled())
		glDepthMask(GL_TRUE);

	uint32_t numInvalidateAttachments = 0;
	GLenum invalidateAttachments[MAX_COLOR_ATTACHMENTS + 2];

	if (hasDepth)
	{
		if (beginInfo.depthLoadOp == beginInfo.stencilLoadOp && hasStencil)
		{
			if (beginInfo.depthLoadOp == AttachmentLoadOp::Clear || forceClear)
			{
				glClearBufferfi(GL_DEPTH_STENCIL, 0, beginInfo.depthClearValue, beginInfo.stencilClearValue);
			}
			else if (beginInfo.depthLoadOp == AttachmentLoadOp::Discard)
			{
				if (beginInfo.framebuffer == nullptr && defaultFramebuffer == 0)
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
			if (beginInfo.depthLoadOp == AttachmentLoadOp::Clear || forceClear)
			{
				glClearBufferfv(GL_DEPTH, 0, &beginInfo.depthClearValue);
			}
			else if (beginInfo.depthLoadOp == AttachmentLoadOp::Discard)
			{
				if (beginInfo.framebuffer == nullptr && defaultFramebuffer == 0)
				{
					invalidateAttachments[numInvalidateAttachments++] = GL_COLOR;
				}
				else
				{
					invalidateAttachments[numInvalidateAttachments++] = GL_DEPTH_ATTACHMENT;
				}
			}

			if (hasStencil)
			{
				if (beginInfo.stencilLoadOp == AttachmentLoadOp::Clear || forceClear)
				{
					GLuint value = beginInfo.stencilClearValue;
					glClearBufferuiv(GL_STENCIL, 0, &value);
				}
				else if (beginInfo.stencilLoadOp == AttachmentLoadOp::Discard)
				{
					if (beginInfo.framebuffer == nullptr && defaultFramebuffer == 0)
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

	for (uint32_t i = 0; i < numColorAttachments; i++)
	{
		const RenderPassColorAttachment& attachment = beginInfo.colorAttachments[i];
		if (attachment.loadOp == AttachmentLoadOp::Clear || forceClear)
		{
			FormatType expectedFormatType = FormatType::Float;
			if (currentFramebuffer != nullptr)
				expectedFormatType = GetFormatType(currentFramebuffer->colorAttachmentFormats[i]);

			if (expectedFormatType == FormatType::UInt)
			{
				std::array<GLuint, 4> clearValue = GetClearValueAs<GLuint>(attachment.clearValue);
				glClearBufferuiv(GL_COLOR, i, clearValue.data());
			}
			else if (expectedFormatType == FormatType::SInt)
			{
				std::array<GLint, 4> clearValue = GetClearValueAs<GLint>(attachment.clearValue);
				glClearBufferiv(GL_COLOR, i, clearValue.data());
			}
			else
			{
				std::array<float, 4> clearValue = GetClearValueAs<float>(attachment.clearValue);
				glClearBufferfv(GL_COLOR, i, clearValue.data());
			}
		}
		else if (attachment.loadOp == AttachmentLoadOp::Discard)
		{
			if (beginInfo.framebuffer == nullptr && defaultFramebuffer == 0)
			{
				invalidateAttachments[numInvalidateAttachments++] = GL_COLOR;
			}
			else
			{
				invalidateAttachments[numInvalidateAttachments++] = GL_COLOR_ATTACHMENT0 + i;
			}
		}
	}

	if (numInvalidateAttachments != 0)
	{
		glInvalidateFramebuffer(GL_FRAMEBUFFER, numInvalidateAttachments, invalidateAttachments);
	}

	InitScissorTest();
	if (!IsDepthWriteEnabled())
		glDepthMask(GL_FALSE);

	hasWrittenToBackBuffer = true;
}

void EndRenderPass(CommandContextHandle)
{
	AssertRenderPassActive("EndRenderPass");
	isInsideRenderPass = false;
	if (currentFramebuffer != nullptr)
	{
		for (const auto& resolveFBO : currentFramebuffer->resolveFBOs)
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFBO.framebuffers[0]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO.framebuffers[1]);
			glBlitFramebuffer(
				0, 0, currentFramebuffer->width, currentFramebuffer->height, 0, 0, currentFramebuffer->width,
				currentFramebuffer->height, resolveFBO.mask, GL_NEAREST);
		}
	}
}

void GLESAssertTextureBindNotInCurrentFramebuffer(const Texture& texture)
{
	if (!useGLESPath || !DevMode() || currentFramebuffer == nullptr)
		return;
	for (const Texture* attachment : currentFramebuffer->attachments)
	{
		if (&texture == attachment)
		{
			std::string labelStringToInsert = Concat({ " [", texture.label, "]" });

			EG_PANIC(
				"Attampted to bind texture for reading "
				<< labelStringToInsert
				<< "while it is part of a framebuffer attachment. "
				   "This might be valid in desktop GL if the subresource is different but it is not valid in GLES.");
		}
	}
}

GLuint srgbEmulationTexture;
int srgbEmulationTextureWidth = -1;
int srgbEmulationTextureHeight = -1;

void UpdateSRGBEmulationTexture(int width, int height)
{
	if (enableDefaultFramebufferSRGBEmulation &&
	    (srgbEmulationTextureWidth != width || srgbEmulationTextureHeight != height))
	{
		if (defaultFramebuffer != 0)
		{
			glDeleteFramebuffers(1, &defaultFramebuffer);
			glDeleteTextures(1, &srgbEmulationTexture);
		}

		glGenFramebuffers(1, &defaultFramebuffer);

		glGenTextures(1, &srgbEmulationTexture);
		glBindTexture(GL_TEXTURE_2D, srgbEmulationTexture);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, width, height);

		glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srgbEmulationTexture, 0);
		GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
		glDrawBuffers(1, &drawBuffer);
		AssertFramebufferComplete(GL_FRAMEBUFFER);
		BindCorrectFramebuffer();

		srgbEmulationTextureWidth = width;
		srgbEmulationTextureHeight = height;
	}
}

static GLuint fixSrgbShader;

static void LoadFixSRGBShader()
{
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	CompileShaderStage(
		vertexShader,
		R"(#version 300 es
const vec2 positions[] = vec2[](vec2(-1, -1),vec2(-1, 3),vec2(3, -1));
out vec2 vTexCoord;
void main() {
	gl_Position = vec4(positions[gl_VertexID], 0, 1);
	vTexCoord = gl_Position.xy * vec2(0.5, 0.5) + vec2(0.5);
})");

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	CompileShaderStage(
		fragmentShader,
		R"(#version 300 es
precision highp float;
uniform sampler2D t;
in vec2 vTexCoord;
out vec4 color;
void main() {
	vec4 c = texture(t,vTexCoord);
	bvec4 cutoff = lessThan(c, vec4(0.0031308));
	vec4 higher = vec4(1.055)*pow(c, vec4(1.0/2.4)) - vec4(0.055);
	vec4 lower = c * vec4(12.92);
	color = mix(higher, lower, cutoff);
})");

	fixSrgbShader = glCreateProgram();
	glAttachShader(fixSrgbShader, vertexShader);
	glAttachShader(fixSrgbShader, fragmentShader);

	LinkShaderProgram(fixSrgbShader);
}

void SRGBEmulationEndFrame()
{
	if (enableDefaultFramebufferSRGBEmulation && defaultFramebuffer != 0)
	{
		if (fixSrgbShader == 0)
		{
			LoadFixSRGBShader();
		}

		glUseProgram(fixSrgbShader);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, srgbEmulationTexture);

		glViewport(0, 0, srgbEmulationTextureWidth, srgbEmulationTextureHeight);
		SetEnabled<GL_SCISSOR_TEST>(false);

		glDrawArrays(GL_TRIANGLES, 0, 3);

		currentPipeline = nullptr;
		viewportOutOfDate = true;
	}
}

FramebufferFormat FramebufferFormat::GetCurrent()
{
	if (currentFramebuffer == nullptr)
	{
		static eg::Format defaultFramebufferColorFormat = eg::Format::DefaultColor;
		return FramebufferFormat{
			.colorAttachmentFormats = std::span<const Format>(&defaultFramebufferColorFormat, 1),
			.depthStencilAttachmentFormat = eg::Format::DefaultDepthStencil,
			.sampleCount = 1,
		};
	}

	return FramebufferFormat{
		.colorAttachmentFormats = std::span<const Format>(
			currentFramebuffer->colorAttachmentFormats, currentFramebuffer->numColorAttachments),
		.depthStencilAttachmentFormat = currentFramebuffer->depthStencilAttachmentFormat,
		.sampleCount = currentFramebuffer->sampleCount,
	};
}

size_t FramebufferFormat::Hash() const
{
	size_t h = sampleCount | colorAttachmentFormats.size() << 16;
	for (eg::Format format : colorAttachmentFormats)
		HashAppend(h, format);
	HashAppend(h, depthStencilAttachmentFormat.value_or(eg::Format::Undefined));
	return h;
}

void FramebufferFormat::PrintToStdout(std::string_view linePrefix, const FramebufferFormat* expected) const
{
	constexpr const char* ANSI_BOLD_ON = "\x1b[1m";
	constexpr const char* ANSI_BOLD_OFF = "\x1b[22m";

	std::cout << linePrefix << "samples: " << ANSI_BOLD_ON << sampleCount << ANSI_BOLD_OFF << "\n";

	for (size_t i = 0; i < colorAttachmentFormats.size(); i++)
	{
		std::cout << linePrefix << "color[" << i << "]: " << FormatToString(colorAttachmentFormats[i]);

		if (expected != nullptr)
		{
			eg::Format expectedFormat = expected->colorAttachmentFormats.size() < i
			                                ? eg::Format::Undefined
			                                : expected->colorAttachmentFormats[i];
			if (expected->colorAttachmentFormats[i] != colorAttachmentFormats[i])
			{
				std::cout << ANSI_BOLD_ON << " MISMATCH! PSO:" << FormatToString(expectedFormat) << ANSI_BOLD_OFF;
			}
		}

		std::cout << "\n";
	}

	auto GetOptFormatName = [&](std::optional<Format> format) -> std::string_view
	{
		if (format.has_value())
			return FormatToString(*format);
		else
			return "none";
	};

	std::cout << linePrefix << "depth: " << GetOptFormatName(depthStencilAttachmentFormat);
	if (expected != nullptr && expected->depthStencilAttachmentFormat != depthStencilAttachmentFormat)
	{
		std::cout << ANSI_BOLD_ON << " MISMATCH! PSO:" << GetOptFormatName(expected->depthStencilAttachmentFormat)
				  << ANSI_BOLD_OFF;
	}

	std::cout << "\n";
}
} // namespace eg::graphics_api::gl
