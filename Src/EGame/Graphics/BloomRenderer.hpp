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
			
		private:
			friend class BloomRenderer;
			
			Texture m_mainTexture;
			Texture m_auxTexture;
		};
		
		static constexpr Format FORMAT = Format::R16G16B16A16_Float;
		
		BloomRenderer();
		
		void Render(const glm::vec3& threshold, eg::TextureRef inputTexture, RenderTarget& renderTarget) const;
		
	private:
		Pipeline m_brightPassPipeline;
		Pipeline m_blurPipeline;
		Pipeline m_upscalePipeline;
		
		Sampler m_inputSampler;
	};
}
