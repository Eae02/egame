#include "SpriteBatch.hpp"
#include "SpriteFont.hpp"
#include "Graphics.hpp"
#include "../Utils.hpp"
#include "../../Shaders/Build/Sprite.vs.h"
#include "../../Shaders/Build/Sprite.fs.h"

#include <glm/gtx/matrix_transform_2d.hpp>
#include <utf8.h>

namespace eg
{
	SpriteBatch SpriteBatch::overlay;
	
	static Pipeline spritePipeline;
	static Texture whitePixelTexture;
	
	void SpriteBatch::InitStatic()
	{
		ShaderModule vs(ShaderStage::Vertex, Sprite_vs_glsl);
		ShaderModule fs(ShaderStage::Fragment, Sprite_fs_glsl);
		
		GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = vs.Handle();
		pipelineCI.fragmentShader = fs.Handle();
		pipelineCI.enableScissorTest = true;
		pipelineCI.blendStates[0] = AlphaBlend;
		pipelineCI.vertexBindings[0] = { sizeof(Vertex), InputRate::Vertex };
		pipelineCI.vertexAttributes[0] = { 0, DataType::Float32, 2, (uint32_t)offsetof(Vertex, position) };
		pipelineCI.vertexAttributes[1] = { 0, DataType::Float32, 2, (uint32_t)offsetof(Vertex, texCoord) };
		pipelineCI.vertexAttributes[2] = { 0, DataType::UInt8Norm, 4, (uint32_t)offsetof(Vertex, color) };
		pipelineCI.label = "SpriteBatch";
		spritePipeline = eg::Pipeline::Create(pipelineCI);
		
		SamplerDescription whiteTexSamplerDesc;
		TextureCreateInfo whiteTexCreateInfo;
		whiteTexCreateInfo.width = 1;
		whiteTexCreateInfo.height = 1;
		whiteTexCreateInfo.mipLevels = 1;
		whiteTexCreateInfo.format = Format::R8G8B8A8_UNorm;
		whiteTexCreateInfo.defaultSamplerDescription = &whiteTexSamplerDesc;
		whiteTexCreateInfo.flags = TextureFlags::ShaderSample | TextureFlags::CopyDst;
		whitePixelTexture = Texture::Create2D(whiteTexCreateInfo);
		
		UploadBuffer uploadBuffer = GetTemporaryUploadBuffer(4);
		uint8_t* uploadBufferMem = static_cast<uint8_t*>(uploadBuffer.Map());
		std::fill_n(uploadBufferMem, 4, 255);
		uploadBuffer.Flush();
		
		TextureRange textureRange = { };
		textureRange.sizeX = 1;
		textureRange.sizeY = 1;
		textureRange.sizeZ = 1;
		DC.SetTextureData(whitePixelTexture, textureRange, uploadBuffer.buffer, uploadBuffer.offset);
		
		whitePixelTexture.UsageHint(TextureUsage::ShaderSample, ShaderAccessFlags::Fragment);
	}
	
	void SpriteBatch::DestroyStatic()
	{
		whitePixelTexture.Destroy();
		spritePipeline.Destroy();
	}
	
	void SpriteBatch::Begin()
	{
		m_batches.clear();
		m_indices.clear();
		m_vertices.clear();
		opacityScale = 1;
	}
	
	void SpriteBatch::InitBatch(const Texture& texture, SpriteFlags flags)
	{
		bool redToAlpha = HasFlag(flags, SpriteFlags::RedToAlpha);
		uint32_t mipLevel = HasFlag(flags, SpriteFlags::ForceLowestMipLevel) ? texture.MipLevels() - 1 : 0;
		
		bool needsNewBatch = true;
		if (!m_batches.empty() && m_batches.back().texture.handle == texture.handle &&
			m_batches.back().redToAlpha == redToAlpha && m_batches.back().mipLevel == mipLevel)
		{
			if (!m_batches.back().enableScissor && m_scissorStack.empty())
			{
				needsNewBatch = false;
			}
			else if (m_batches.back().enableScissor && !m_scissorStack.empty() &&
			         m_batches.back().scissor == m_scissorStack.top())
			{
				needsNewBatch = false;
			}
		}
		
		if (needsNewBatch)
		{
			Batch& batch = m_batches.emplace_back();
			batch.redToAlpha = redToAlpha;
			batch.mipLevel = mipLevel;
			batch.texture = texture;
			batch.firstIndex = static_cast<uint32_t>(m_indices.size());
			batch.numIndices = 0;
			if ((batch.enableScissor = !m_scissorStack.empty()))
			{
				batch.scissor = m_scissorStack.top();
			}
		}
	}
	
	void SpriteBatch::AddQuadIndices()
	{
		uint32_t i0 = static_cast<uint32_t>(m_vertices.size());
		
		uint32_t indices[] = { 0, 1, 2, 1, 2, 3 };
		for (uint32_t i : indices)
		{
			m_indices.push_back(i0 + i);
		}
		
		m_batches.back().numIndices += 6;
	}
	
	void SpriteBatch::Draw(const Texture& texture, const glm::vec2& position, const ColorLin& color,
		const Rectangle& texRectangle, float scale, SpriteFlags spriteFlags, float rotation, glm::vec2 origin)
	{
		InitBatch(texture, spriteFlags);
		
		AddQuadIndices();
		
		float uOffsets[] = { 0, texRectangle.w };
		float vOffsets[] = { 0, texRectangle.h };
		
		if ((int)spriteFlags & (int)SpriteFlags::FlipX)
		{
			std::swap(uOffsets[0], uOffsets[1]);
			origin.x = texRectangle.w - origin.x;
		}
		
		if ((int)spriteFlags & (int)SpriteFlags::FlipY)
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
				const float u = (texRectangle.x + uOffsets[x]) / (float)texture.Width();
				const float v = (texRectangle.y + vOffsets[y]) / (float)texture.Height();
				
				const float offX = texRectangle.w * (float)x - origin.x;
				const float offY = -(texRectangle.h * (float)y - origin.y);
				const float rOffX = offX * cosR - offY * sinR;
				const float rOffY = offX * sinR + offY * cosR;
				
				m_vertices.emplace_back(position + glm::vec2(rOffX, rOffY) * scale, glm::vec2(u, v), color, opacityScale);
			}
		}
	}
	
	void SpriteBatch::Draw(const Texture& texture, const Rectangle& rectangle, const ColorLin& color,
		const Rectangle& texRectangle, SpriteFlags spriteFlags)
	{
		InitBatch(texture, spriteFlags);
		
		AddQuadIndices();
		
		float uOffsets[] = { 0, texRectangle.w };
		float vOffsets[] = { texRectangle.h, 0 };
		
		if ((int)spriteFlags & (int)SpriteFlags::FlipX)
			std::swap(uOffsets[0], uOffsets[1]);
		if ((int)spriteFlags & (int)SpriteFlags::FlipY)
			std::swap(vOffsets[0], vOffsets[1]);
		
		for (int x = 0; x < 2; x++)
		{
			for (int y = 0; y < 2; y++)
			{
				const float u = (texRectangle.x + uOffsets[x]) / (float)texture.Width();
				const float v = (texRectangle.y + vOffsets[y]) / (float)texture.Height();
				m_vertices.emplace_back(
					glm::vec2(rectangle.x + rectangle.w * (float)x, rectangle.y + rectangle.h * (float)y),
					glm::vec2(u, v), color, opacityScale);
			}
		}
	}
	
	void SpriteBatch::Draw(const Texture& texture, const Rectangle& rectangle, const ColorLin& color, SpriteFlags spriteFlags)
	{
		InitBatch(texture, spriteFlags);
		
		AddQuadIndices();
		
		float uOffsets[] = { 0, 1 };
		float vOffsets[] = { 1, 0 };
		if ((int)spriteFlags & (int)SpriteFlags::FlipX)
			std::swap(uOffsets[0], uOffsets[1]);
		if ((int)spriteFlags & (int)SpriteFlags::FlipY)
			std::swap(vOffsets[0], vOffsets[1]);
		
		for (int x = 0; x < 2; x++)
		{
			for (int y = 0; y < 2; y++)
			{
				const float u = uOffsets[x];
				const float v = vOffsets[y];
				m_vertices.emplace_back(
					glm::vec2(rectangle.x + rectangle.w * (float)x, rectangle.y + rectangle.h * (float)y),
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
			m_vertices.emplace_back(begin + dO * (width * (float)(s * 2 - 1)), glm::vec2(0, 0), color, opacityScale);
		}
		for (int s = 0; s < 2; s++)
		{
			m_vertices.emplace_back(end + dO * (width * (float)(s * 2 - 1)), glm::vec2(0, 0), color, opacityScale);
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
					glm::vec2(rectangle.x + rectangle.w * (float)x, rectangle.y + rectangle.h * (float)y),
					glm::vec2(0, 0), color, opacityScale);
			}
		}
	}
	
	void SpriteBatch::DrawTextMultiline(const class SpriteFont& font, std::string_view text, const glm::vec2& position,
		const ColorLin& color, float scale, float lineSpacing, glm::vec2* sizeOut, TextFlags flags, const ColorLin* secondColor)
	{
		float maxW = 0;
		float yOffset = 0;
		
		IterateStringParts(text, '\n', [&] (std::string_view line)
		{
			glm::vec2 lineSize;
			DrawText(font, line, glm::vec2(position.x, position.y - scale - yOffset), color, scale, &lineSize, flags, secondColor);
			yOffset += font.LineHeight() * scale + lineSpacing;
			maxW = std::max(maxW, lineSize.x);
		});
		
		if (sizeOut)
		{
			sizeOut->x = maxW;
			sizeOut->y = yOffset;
		}
	}
	
	void SpriteBatch::DrawText(const SpriteFont& font, std::string_view text, const glm::vec2& position,
		const ColorLin& color, float scale, glm::vec2* sizeOut, TextFlags flags, const ColorLin* secondColor)
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
			rectangle.x = position.x + (x + (float)fontChar.xOffset + (float)kerning) * scale;
			rectangle.y = position.y - (float)(0 - fontChar.yOffset + (int)fontChar.height) * scale;
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
				Draw(font.Tex(), shadowRectangle, eg::ColorLin(0, 0, 0, currentColor->a * 0.5f), srcRectangle, SpriteFlags::RedToAlpha);
			}
			
			Draw(font.Tex(), rectangle, *currentColor, srcRectangle, SpriteFlags::RedToAlpha);
			
			x += fontChar.xAdvance + (float)kerning;
			sizeOut->y = std::max(sizeOut->y, rectangle.h);
		}
		
		sizeOut->x = x * scale;
	}
	
	void SpriteBatch::End(int screenWidth, int screenHeight, const RenderPassBeginInfo& rpBeginInfo, const glm::mat3& matrix)
	{
		if (m_batches.empty())
			return;
		
		//Reallocates the vertex buffer if it's too small
		if (m_vertexBufferCapacity < m_vertices.size())
		{
			m_vertexBufferCapacity = RoundToNextMultiple((uint32_t)m_vertices.size(), 1024);
			m_vertexBuffer = Buffer(BufferFlags::CopyDst | BufferFlags::VertexBuffer,
				m_vertexBufferCapacity * sizeof(Vertex), nullptr);
		}
		
		//Reallocates the index buffer if it's too small
		if (m_indexBufferCapacity < m_indices.size())
		{
			m_indexBufferCapacity = RoundToNextMultiple((uint32_t)m_indices.size(), 1024);
			m_indexBuffer = Buffer(BufferFlags::CopyDst | BufferFlags::IndexBuffer,
				m_indexBufferCapacity * sizeof(uint32_t), nullptr);
		}
		
		//Copies vertices and indices to an upload buffer
		const size_t verticesBytes = m_vertices.size() * sizeof(Vertex);
		const size_t indicesBytes = m_indices.size() * sizeof(uint32_t);
		UploadBuffer uploadBuffer = GetTemporaryUploadBuffer(verticesBytes + indicesBytes);
		char* uploadMem = static_cast<char*>(uploadBuffer.Map());
		std::memcpy(uploadMem, m_vertices.data(), verticesBytes);
		std::memcpy(uploadMem + verticesBytes, m_indices.data(), indicesBytes);
		uploadBuffer.Flush();
		
		//Copies vertices and indices to the GPU buffers
		DC.CopyBuffer(uploadBuffer.buffer, m_vertexBuffer, uploadBuffer.offset, 0, verticesBytes);
		DC.CopyBuffer(uploadBuffer.buffer, m_indexBuffer, uploadBuffer.offset + verticesBytes, 0, indicesBytes);
		
		m_vertexBuffer.UsageHint(BufferUsage::VertexBuffer);
		m_indexBuffer.UsageHint(BufferUsage::IndexBuffer);
		
		DC.BeginRenderPass(rpBeginInfo);
		
		DC.BindPipeline(spritePipeline);
		
		float pcData[4 * 3];
		for (int r = 0; r < 3; r++)
		{
			for (int c = 0; c < 3; c++)
				pcData[r * 4 + c] = matrix[r][c];
		}
		DC.PushConstants(0, sizeof(pcData), pcData);
		
		DC.BindIndexBuffer(IndexType::UInt32, m_indexBuffer, 0);
		DC.BindVertexBuffer(0, m_vertexBuffer, 0);
		
		for (const Batch& batch : m_batches)
		{
			int32_t pc = (int32_t)batch.redToAlpha;
			DC.PushConstants(64, sizeof(pc), &pc);
			
			if (batch.enableScissor)
				DC.SetScissor(batch.scissor.x, batch.scissor.y, batch.scissor.width, batch.scissor.height);
			else
				DC.SetScissor(0, 0, screenWidth, screenHeight);
			
			eg::TextureSubresource subres;
			subres.firstMipLevel = batch.mipLevel;
			DC.BindTexture(batch.texture, 0, 0, nullptr, subres);
			
			DC.DrawIndexed(batch.firstIndex, batch.numIndices, 0, 0, 1);
		}
		
		DC.EndRenderPass();
	}
	
	void SpriteBatch::End(int screenWidth, int screenHeight, const RenderPassBeginInfo& rpBeginInfo)
	{
		glm::mat3 transform = glm::translate(glm::mat3(1), glm::vec2(-1)) *
			glm::scale(glm::mat3(1), glm::vec2(2.0f / (float)screenWidth, 2.0f / (float)screenHeight));
		
		End(screenWidth, screenHeight, rpBeginInfo, transform);
	}
	
	void SpriteBatch::PushScissorF(float x, float y, float width, float height)
	{
		PushScissor((int)std::round(x), (int)std::round(y), (int)std::ceil(width), (int)std::ceil(height));
	}
	
	void SpriteBatch::PushScissor(int x, int y, int width, int height)
	{
		if (m_scissorStack.empty())
		{
			m_scissorStack.push({x, y, width, height});
		}
		else
		{
			int ix = std::max(x, m_scissorStack.top().x);
			int iy = std::max(y, m_scissorStack.top().y);
			int iw = std::min(x + width, m_scissorStack.top().x + m_scissorStack.top().width) - ix;
			int ih = std::min(y + height, m_scissorStack.top().y + m_scissorStack.top().height) - iy;
			m_scissorStack.push({ ix, iy, iw, ih });
		}
	}
	
	SpriteBatch::Vertex::Vertex(const glm::vec2& _position, const glm::vec2& _texCoord, const ColorLin& _color, float opacityScale)
		: position(_position), texCoord(_texCoord)
	{
		color[0] = static_cast<uint8_t>(glm::clamp((int)std::round(_color.r * 255.0f), 0, 255));
		color[1] = static_cast<uint8_t>(glm::clamp((int)std::round(_color.g * 255.0f), 0, 255));
		color[2] = static_cast<uint8_t>(glm::clamp((int)std::round(_color.b * 255.0f), 0, 255));
		color[3] = static_cast<uint8_t>(glm::clamp((int)std::round(_color.a * 255.0f * opacityScale), 0, 255));
	}
}
