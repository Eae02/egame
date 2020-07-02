#pragma once

#include "AbstractionHL.hpp"

namespace eg
{
	class EG_API BloomRenderer
	{
	public:
		class EG_API RenderTarget
		{
		public:
			RenderTarget(uint32_t inputWidth, uint32_t inputHeight, uint32_t levels = 4);
			
			const Texture& OutputTexture() const
			{
				return m_mainTexture;
			}
			
			uint32_t InputWidth() const { return m_inputWidth; }
			uint32_t InputHeight() const { return m_inputHeight; }
			
		private:
			friend class BloomRenderer;
			
			uint32_t m_inputWidth;
			uint32_t m_inputHeight;
			
			Texture m_mainTexture;
			Texture m_auxTexture;
		};
		
		static constexpr Format FORMAT = Format::R16G16B16A16_Float;
		
		BloomRenderer();
		
		void Render(const glm::vec3& threshold, eg::TextureRef inputTexture, RenderTarget& renderTarget) const;
		
	private:
		uint32_t m_workGroupSizeY;
		
		Pipeline m_brightPassPipeline;
		Pipeline m_blurPipeline;
		Pipeline m_upscalePipeline;
		
		Sampler m_inputSampler;
	};
}
