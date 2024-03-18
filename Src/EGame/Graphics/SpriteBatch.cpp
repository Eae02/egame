#include "SpriteBatch.hpp"
#include "../../Shaders/Build/Sprite.fs.h"
#include "../../Shaders/Build/Sprite.vs.h"
#include "../Color.hpp"
#include "../String.hpp"
#include "FramebufferLazyPipeline.hpp"
#include "Graphics.hpp"
#include "SpriteFont.hpp"

#include <glm/gtx/matrix_transform_2d.hpp>
#include <utf8.h>

namespace eg
{
SpriteBatch SpriteBatch::overlay;

static FramebufferLazyPipeline spritePipeline;
static Texture whitePixelTexture;
static Sampler spriteBatchSampler;

static ShaderModule spriteBatchVS;
static ShaderModule spriteBatchFS;

static Buffer flagsUniformBuffer;
static uint32_t flagsUniformBufferBytesPerFlag;

static constexpr uint32_t NUM_FLAG_COMBINATIONS = 1 << 3;

static const DescriptorSetBinding bindingsSet0[] = {
	{
		.binding = 0,
		.type = BindingType::UniformBuffer,
		.shaderAccess = ShaderAccessFlags::Vertex,
	},
	{
		.binding = 1,
		.type = BindingType::UniformBufferDynamicOffset,
		.shaderAccess = ShaderAccessFlags::Fragment,
	},
};

void SpriteBatch::InitStatic()
{
	spriteBatchVS = ShaderModule(ShaderStage::Vertex, Sprite_vs_glsl);
	spriteBatchFS = ShaderModule(ShaderStage::Fragment, Sprite_fs_glsl);

	spritePipeline = FramebufferLazyPipeline({
		.vertexShader.shaderModule = spriteBatchVS.Handle(),
		.fragmentShader.shaderModule = spriteBatchFS.Handle(),
		.enableScissorTest = true,
		.setBindModes = {eg::BindMode::DescriptorSet, eg::BindMode::DescriptorSet},
		.descriptorSetBindings = { bindingsSet0 },
		.blendStates = {eg::BlendState(BlendFunc::Add, BlendFactor::One, BlendFactor::OneMinusSrcAlpha)},
		.vertexBindings = {VertexBinding(sizeof(Vertex), InputRate::Vertex)},
		.vertexAttributes = {
			VertexAttribute(0, DataType::Float32, 2, offsetof(Vertex, position)),
			VertexAttribute(0, DataType::Float32, 2, offsetof(Vertex, texCoord)),
			VertexAttribute(0, DataType::UInt8Norm, 4, offsetof(Vertex, color)),
		},
		.label = "SpriteBatch"
	});

	whitePixelTexture = Texture::Create2D({
		.flags = TextureFlags::ShaderSample | TextureFlags::CopyDst,
		.mipLevels = 1,
		.width = 1,
		.height = 1,
		.format = Format::R8G8B8A8_UNorm,
	});

	UploadBuffer uploadBuffer = GetTemporaryUploadBuffer(4);
	uint8_t* uploadBufferMem = static_cast<uint8_t*>(uploadBuffer.Map());
	std::fill_n(uploadBufferMem, 4, 255);
	uploadBuffer.Flush();

	DC.SetTextureData(
		whitePixelTexture, { .sizeX = 1, .sizeY = 1, .sizeZ = 1 }, uploadBuffer.buffer, uploadBuffer.offset);

	whitePixelTexture.UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Fragment);

	spriteBatchSampler = eg::Sampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::ClampToEdge,
		.wrapV = eg::WrapMode::ClampToEdge,
		.wrapW = eg::WrapMode::ClampToEdge,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Linear,
	});

	flagsUniformBufferBytesPerFlag =
		std::max((uint32_t)sizeof(uint32_t), GetGraphicsDeviceInfo().uniformBufferOffsetAlignment);
	uint32_t wordsPerFlag = flagsUniformBufferBytesPerFlag / sizeof(uint32_t);
	std::vector<uint32_t> flagsBufferData(wordsPerFlag * NUM_FLAG_COMBINATIONS);
	for (uint32_t i = 0; i < NUM_FLAG_COMBINATIONS; i++)
		flagsBufferData[i * wordsPerFlag] = i;
	flagsUniformBuffer = eg::Buffer(
		eg::BufferFlags::UniformBuffer, flagsUniformBufferBytesPerFlag * NUM_FLAG_COMBINATIONS, flagsBufferData.data());
}

void SpriteBatch::DestroyStatic()
{
	spriteBatchVS.Destroy();
	spriteBatchFS.Destroy();
	whitePixelTexture.Destroy();
	spritePipeline.DestroyPipelines();
	flagsUniformBuffer.Destroy();
}

void SpriteBatch::PushBlendState(SpriteBlend blendState)
{
	m_blendStateStack.push_back(blendState);
}

void SpriteBatch::PopBlendState()
{
	if (m_blendStateStack.size() <= 1)
	{
		EG_PANIC("SpriteBatch::PopBlendState called while blend state stack is empty");
	}
	m_blendStateStack.pop_back();
}

void SpriteBatch::InitBatch(const Texture& texture, SpriteFlags flags)
{
	bool redToAlpha = HasFlag(flags, SpriteFlags::RedToAlpha);
	uint32_t mipLevel = HasFlag(flags, SpriteFlags::ForceLowestMipLevel) ? texture.MipLevels() - 1 : 0;

	DescriptorSetRef textureDescriptorSet = texture.GetFragmentShaderSampleDescriptorSet();

	bool needsNewBatch = true;
	if (!m_batches.empty() && m_batches.back().textureDescriptorSet.handle == textureDescriptorSet.handle &&
	    m_batches.back().redToAlpha == redToAlpha && m_batches.back().mipLevel == mipLevel &&
	    m_batches.back().blend == m_blendStateStack.back())
	{
		if (!m_batches.back().enableScissor && m_scissorStack.empty())
		{
			needsNewBatch = false;
		}
		else if (
			m_batches.back().enableScissor && !m_scissorStack.empty() &&
			m_batches.back().scissor == m_scissorStack.back())
		{
			needsNewBatch = false;
		}
	}

	if (needsNewBatch)
	{
		Batch& batch = m_batches.emplace_back();
		batch.redToAlpha = redToAlpha;
		batch.mipLevel = mipLevel;
		batch.textureDescriptorSet = textureDescriptorSet;
		batch.blend = m_blendStateStack.back();
		batch.firstIndex = UnsignedNarrow<uint32_t>(m_indices.size());
		batch.numIndices = 0;
		if ((batch.enableScissor = !m_scissorStack.empty()))
		{
			batch.scissor = m_scissorStack.back();
		}
	}
}

void SpriteBatch::AddQuadIndices()
{
	uint32_t i0 = UnsignedNarrow<uint32_t>(m_vertices.size());

	uint32_t indices[] = { 0, 1, 2, 1, 2, 3 };
	for (uint32_t i : indices)
	{
		m_indices.push_back(i0 + i);
	}

	m_batches.back().numIndices += 6;
}

static inline bool ShouldFlipY(SpriteFlags flags)
{
	if (HasFlag(flags, SpriteFlags::FlipY))
		return true;
	if (HasFlag(flags, SpriteFlags::FlipYIfOpenGL) && CurrentGraphicsAPI() == GraphicsAPI::OpenGL)
		return true;
	return false;
}

void SpriteBatch::Draw(
	const Texture& texture, const glm::vec2& position, const ColorLin& color, const Rectangle& texRectangle,
	float scale, SpriteFlags spriteFlags, float rotation, glm::vec2 origin)
{
	InitBatch(texture, spriteFlags);

	AddQuadIndices();

	float uOffsets[] = { 0, texRectangle.w };
	float vOffsets[] = { 0, texRectangle.h };

	if (HasFlag(spriteFlags, SpriteFlags::FlipX))
	{
		std::swap(uOffsets[0], uOffsets[1]);
		origin.x = texRectangle.w - origin.x;
	}

	if (ShouldFlipY(spriteFlags))
	{
		std::swap(vOffsets[0], vOffsets[1]);
		origin.y = texRectangle.h - origin.y;
	}

	const float cosR = std::cos(rotation);
	const float sinR = std::sin(rotation);

	for (int x = 0; x < 2; x++)
	{
		for (int y = 0; y < 2; y++)
		{
			const float u = (texRectangle.x + uOffsets[x]) / static_cast<float>(texture.Width());
			const float v = (texRectangle.y + vOffsets[y]) / static_cast<float>(texture.Height());

			const float offX = texRectangle.w * static_cast<float>(x) - origin.x;
			const float offY = -(texRectangle.h * static_cast<float>(y) - origin.y);
			const float rOffX = offX * cosR - offY * sinR;
			const float rOffY = offX * sinR + offY * cosR;

			m_vertices.emplace_back(position + glm::vec2(rOffX, rOffY) * scale, glm::vec2(u, v), color, opacityScale);
		}
	}
}

void SpriteBatch::Draw(
	const Texture& texture, const Rectangle& rectangle, const ColorLin& color, const Rectangle& texRectangle,
	SpriteFlags spriteFlags)
{
	InitBatch(texture, spriteFlags);

	AddQuadIndices();

	float uOffsets[] = { 0, texRectangle.w };
	float vOffsets[] = { texRectangle.h, 0 };

	if (HasFlag(spriteFlags, SpriteFlags::FlipX))
		std::swap(uOffsets[0], uOffsets[1]);
	if (ShouldFlipY(spriteFlags))
		std::swap(vOffsets[0], vOffsets[1]);

	for (int x = 0; x < 2; x++)
	{
		for (int y = 0; y < 2; y++)
		{
			const float u = (texRectangle.x + uOffsets[x]) / static_cast<float>(texture.Width());
			const float v = (texRectangle.y + vOffsets[y]) / static_cast<float>(texture.Height());
			m_vertices.emplace_back(
				glm::vec2(
					rectangle.x + rectangle.w * static_cast<float>(x),
					rectangle.y + rectangle.h * static_cast<float>(y)),
				glm::vec2(u, v), color, opacityScale);
		}
	}
}

void SpriteBatch::Draw(
	const Texture& texture, const Rectangle& rectangle, const ColorLin& color, SpriteFlags spriteFlags)
{
	InitBatch(texture, spriteFlags);

	AddQuadIndices();

	float uOffsets[] = { 0, 1 };
	float vOffsets[] = { 1, 0 };
	if (HasFlag(spriteFlags, SpriteFlags::FlipX))
		std::swap(uOffsets[0], uOffsets[1]);
	if (ShouldFlipY(spriteFlags))
		std::swap(vOffsets[0], vOffsets[1]);

	for (int x = 0; x < 2; x++)
	{
		for (int y = 0; y < 2; y++)
		{
			const float u = uOffsets[x];
			const float v = vOffsets[y];
			m_vertices.emplace_back(
				glm::vec2(
					rectangle.x + rectangle.w * static_cast<float>(x),
					rectangle.y + rectangle.h * static_cast<float>(y)),
				glm::vec2(u, v), color, opacityScale);
		}
	}
}

void SpriteBatch::DrawRectBorder(const Rectangle& rectangle, const ColorLin& color, float width)
{
	DrawLine({ rectangle.x, rectangle.y }, { rectangle.MaxX(), rectangle.y }, color, width);
	DrawLine({ rectangle.MaxX(), rectangle.y }, { rectangle.x + rectangle.w, rectangle.MaxY() }, color, width);
	DrawLine({ rectangle.MaxX(), rectangle.MaxY() }, { rectangle.x, rectangle.MaxY() }, color, width);
	DrawLine({ rectangle.x, rectangle.MaxY() }, { rectangle.x, rectangle.y }, color, width);
}

void SpriteBatch::DrawLine(const glm::vec2& begin, const glm::vec2& end, const ColorLin& color, float width)
{
	InitBatch(whitePixelTexture, SpriteFlags::None);

	AddQuadIndices();

	glm::vec2 d = glm::normalize(end - begin);
	glm::vec2 dO(d.y, -d.x);

	for (int s = 0; s < 2; s++)
	{
		m_vertices.emplace_back(begin + dO * (width * static_cast<float>(s * 2 - 1)), glm::vec2(), color, opacityScale);
	}
	for (int s = 0; s < 2; s++)
	{
		m_vertices.emplace_back(end + dO * (width * static_cast<float>(s * 2 - 1)), glm::vec2(), color, opacityScale);
	}
}

void SpriteBatch::DrawCustomShape(
	std::span<const glm::vec2> positions, std::span<const uint32_t> indices, const ColorLin& color)
{
	InitBatch(whitePixelTexture, SpriteFlags::None);

	uint32_t i0 = UnsignedNarrow<uint32_t>(m_vertices.size());
	for (uint32_t i : indices)
		m_indices.push_back(i0 + i);

	m_batches.back().numIndices += UnsignedNarrow<uint32_t>(indices.size());

	for (glm::vec2 pos : positions)
	{
		m_vertices.emplace_back(pos, glm::vec2(), color, opacityScale);
	}
}

void SpriteBatch::DrawRect(const Rectangle& rectangle, const ColorLin& color)
{
	InitBatch(whitePixelTexture, SpriteFlags::None);

	AddQuadIndices();

	for (int x = 0; x < 2; x++)
	{
		for (int y = 0; y < 2; y++)
		{
			m_vertices.emplace_back(
				glm::vec2(
					rectangle.x + rectangle.w * static_cast<float>(x),
					rectangle.y + rectangle.h * static_cast<float>(y)),
				glm::vec2(0, 0), color, opacityScale);
		}
	}
}

void SpriteBatch::DrawTextMultiline(
	const class SpriteFont& font, std::string_view text, const glm::vec2& position, const ColorLin& color, float scale,
	float lineSpacing, glm::vec2* sizeOut, TextFlags flags, const ColorLin* secondColor)
{
	float maxW = 0;
	float yOffset = 0;

	IterateStringParts(
		text, '\n',
		[&](std::string_view line)
		{
			glm::vec2 lineSize;
			DrawText(
				font, line, glm::vec2(position.x, position.y - scale - yOffset), color, scale, &lineSize, flags,
				secondColor);
			yOffset += font.LineHeight() * scale + lineSpacing;
			maxW = std::max(maxW, lineSize.x);
		});

	if (sizeOut)
	{
		sizeOut->x = maxW;
		sizeOut->y = yOffset;
	}
}

void SpriteBatch::DrawText(
	const SpriteFont& font, std::string_view text, const glm::vec2& position, const ColorLin& color, float scale,
	glm::vec2* sizeOut, TextFlags flags, const ColorLin* secondColor)
{
	if (sizeOut == nullptr)
	{
		sizeOut = reinterpret_cast<glm::vec2*>(alloca(sizeof(glm::vec2)));
	}

	float x = 0;
	sizeOut->y = 0;

	bool useSecondColor = false;
	const ColorLin* currentColor = &color;
	uint32_t prev = 0;
	for (auto it = text.begin(); it != text.end();)
	{
		const uint32_t c = utf8::unchecked::next(it);
		if (c == ' ')
		{
			x += font.SpaceAdvance();
			continue;
		}
		if (c == '\e')
		{
			if (secondColor != nullptr)
			{
				useSecondColor = !useSecondColor;
				currentColor = useSecondColor ? secondColor : &color;
			}
			continue;
		}

		const Character& fontChar = font.GetCharacterOrDefault(c);

		const int kerning = font.GetKerning(prev, c);

		Rectangle rectangle;
		rectangle.x = position.x + (x + static_cast<float>(fontChar.xOffset) + static_cast<float>(kerning)) * scale;
		rectangle.y = position.y - static_cast<float>(0 - fontChar.yOffset + static_cast<int>(fontChar.height)) * scale;
		rectangle.w = fontChar.width * scale;
		rectangle.h = fontChar.height * scale;

		if (!HasFlag(flags, TextFlags::NoPixelAlign))
		{
			rectangle.x = std::round(rectangle.x);
			rectangle.y = std::round(rectangle.y);
		}

		Rectangle srcRectangle(fontChar.textureX, fontChar.textureY, fontChar.width, fontChar.height);

		if (HasFlag(flags, TextFlags::DropShadow))
		{
			Rectangle shadowRectangle = rectangle;
			shadowRectangle.y -= font.LineHeight() * scale * 0.1f;
			Draw(
				font.Tex(), shadowRectangle, eg::ColorLin(0, 0, 0, currentColor->a * 0.5f), srcRectangle,
				SpriteFlags::RedToAlpha);
		}

		Draw(font.Tex(), rectangle, *currentColor, srcRectangle, SpriteFlags::RedToAlpha);

		x += fontChar.xAdvance + static_cast<float>(kerning);
		sizeOut->y = std::max(sizeOut->y, rectangle.h);
	}

	sizeOut->x = x * scale;
}

void SpriteBatch::Reset()
{
	m_batches.clear();
	m_indices.clear();
	m_vertices.clear();
	m_scissorStack.clear();
	m_blendStateStack.clear();
	m_blendStateStack.push_back(SpriteBlend::Alpha);
	opacityScale = 1;
	m_canRender = false;
}

void SpriteBatch::Upload(const glm::mat3& matrix)
{
	if (m_batches.empty())
		return;

	// Lazily initialize the transform uniform buffer and descriptor set
	if (m_transformUniformBuffer.handle == nullptr)
	{
		m_transformUniformBuffer =
			Buffer(BufferFlags::CopyDst | BufferFlags::UniformBuffer, sizeof(float) * 12, nullptr);

		m_uniformBuffersDescriptorSet = DescriptorSet(bindingsSet0);
		m_uniformBuffersDescriptorSet.BindUniformBuffer(m_transformUniformBuffer, 0);
		m_uniformBuffersDescriptorSet.BindUniformBuffer(
			flagsUniformBuffer, 1, BIND_BUFFER_OFFSET_DYNAMIC, sizeof(uint32_t));
	}

	// Reallocates the vertex buffer if it's too small
	if (m_vertexBufferCapacity < m_vertices.size())
	{
		m_vertexBufferCapacity = RoundToNextMultiple(UnsignedNarrow<uint32_t>(m_vertices.size()), 1024);
		m_vertexBuffer =
			Buffer(BufferFlags::CopyDst | BufferFlags::VertexBuffer, m_vertexBufferCapacity * sizeof(Vertex), nullptr);
	}

	// Reallocates the index buffer if it's too small
	if (m_indexBufferCapacity < m_indices.size())
	{
		m_indexBufferCapacity = RoundToNextMultiple(UnsignedNarrow<uint32_t>(m_indices.size()), 1024);
		m_indexBuffer =
			Buffer(BufferFlags::CopyDst | BufferFlags::IndexBuffer, m_indexBufferCapacity * sizeof(uint32_t), nullptr);
	}

	// Copies vertices and indices to an upload buffer
	const size_t verticesBytes = m_vertices.size() * sizeof(Vertex);
	const size_t indicesBytes = m_indices.size() * sizeof(uint32_t);
	UploadBuffer uploadBuffer = GetTemporaryUploadBuffer(verticesBytes + indicesBytes);
	char* uploadMem = static_cast<char*>(uploadBuffer.Map());
	std::memcpy(uploadMem, m_vertices.data(), verticesBytes);
	std::memcpy(uploadMem + verticesBytes, m_indices.data(), indicesBytes);
	uploadBuffer.Flush();

	// Copies vertices and indices to the GPU buffers
	DC.CopyBuffer(uploadBuffer.buffer, m_vertexBuffer, uploadBuffer.offset, 0, verticesBytes);
	DC.CopyBuffer(uploadBuffer.buffer, m_indexBuffer, uploadBuffer.offset + verticesBytes, 0, indicesBytes);

	m_vertexBuffer.UsageHint(BufferUsage::VertexBuffer);
	m_indexBuffer.UsageHint(BufferUsage::IndexBuffer);

	float matrixPadded[4 * 3] = {};
	for (int r = 0; r < 3; r++)
	{
		for (int c = 0; c < 3; c++)
			matrixPadded[r * 4 + c] = matrix[r][c];
	}
	m_transformUniformBuffer.DCUpdateData<float>(0, matrixPadded);
	m_transformUniformBuffer.UsageHint(BufferUsage::UniformBuffer, ShaderAccessFlags::Vertex);

	m_canRender = true;
}

void SpriteBatch::Upload(float screenWidth, float screenHeight)
{
	glm::vec2 scale(2.0f / screenWidth, 2.0f / screenHeight);
	glm::mat3 matrix = glm::translate(glm::mat3(1), glm::vec2(-1)) * glm::scale(glm::mat3(1), scale);
	Upload(matrix);
}

void SpriteBatch::Render(const RenderArgs& renderArgs) const
{
	if (m_batches.empty())
		return;

	if (!m_canRender)
	{
		EG_PANIC("SpriteBatch::Render called in an invalid state. Did you forget to call SpriteBatch::Upload?");
	}

	spritePipeline.BindPipeline(renderArgs.framebufferFormat);

	int screenWidth = renderArgs.screenWidth.value_or(CurrentResolutionX());
	int screenHeight = renderArgs.screenHeight.value_or(CurrentResolutionY());

	DC.BindIndexBuffer(IndexType::UInt32, m_indexBuffer, 0);
	DC.BindVertexBuffer(0, m_vertexBuffer, 0);

	for (const Batch& batch : m_batches)
	{
		uint32_t flags = static_cast<uint32_t>(batch.blend) | (static_cast<uint32_t>(batch.redToAlpha) << 2);
		EG_ASSERT(flags < NUM_FLAG_COMBINATIONS);

		uint32_t flagsUniformBufferOffset = flags * flagsUniformBufferBytesPerFlag;
		DC.BindDescriptorSet(m_uniformBuffersDescriptorSet, 0, { &flagsUniformBufferOffset, 1 });

		if (batch.enableScissor)
			DC.SetScissor(batch.scissor.x, batch.scissor.y, batch.scissor.width, batch.scissor.height);
		else
			DC.SetScissor(0, 0, screenWidth, screenHeight);

		// TODO: Deal with this now that the texture uses a descriptor set
		eg::TextureSubresource subres;
		if (HasFlag(GetGraphicsDeviceInfo().features, DeviceFeatureFlags::PartialTextureViews))
			subres.firstMipLevel = batch.mipLevel;

		DC.BindDescriptorSet(batch.textureDescriptorSet, 1);

		DC.DrawIndexed(batch.firstIndex, batch.numIndices, 0, 0, 1);
	}
}

void SpriteBatch::UploadAndRender(
	const RenderArgs& renderArgs, const RenderPassBeginInfo& rpBeginInfo, std::optional<glm::mat3> matrix)
{
	if (!m_batches.empty())
	{
		if (matrix.has_value())
		{
			Upload(*matrix);
		}
		else
		{
			Upload(
				renderArgs.screenWidth.value_or(CurrentResolutionX()),
				renderArgs.screenHeight.value_or(CurrentResolutionX()));
		}

		DC.BeginRenderPass(rpBeginInfo);
		Render(renderArgs);
		DC.EndRenderPass();
	}
}

void SpriteBatch::PushScissorF(float x, float y, float width, float height)
{
	PushScissor(
		static_cast<int>(std::round(x)), static_cast<int>(std::round(y)), static_cast<int>(std::ceil(width)),
		static_cast<int>(std::ceil(height)));
}

void SpriteBatch::PushScissor(int x, int y, int width, int height)
{
	if (m_scissorStack.empty())
	{
		m_scissorStack.push_back({ x, y, width, height });
	}
	else
	{
		int ix = std::max(x, m_scissorStack.back().x);
		int iy = std::max(y, m_scissorStack.back().y);
		int iw = std::min(x + width, m_scissorStack.back().x + m_scissorStack.back().width) - ix;
		int ih = std::min(y + height, m_scissorStack.back().y + m_scissorStack.back().height) - iy;
		m_scissorStack.push_back({ ix, iy, iw, ih });
	}
}

SpriteBatch::Vertex::Vertex(
	const glm::vec2& _position, const glm::vec2& _texCoord, const ColorLin& _color, float opacityScale)
	: position(_position), texCoord(_texCoord)
{
	color[0] = ToUNorm8(_color.r);
	color[1] = ToUNorm8(_color.g);
	color[2] = ToUNorm8(_color.b);
	color[3] = ToUNorm8(_color.a * opacityScale);
}
} // namespace eg
