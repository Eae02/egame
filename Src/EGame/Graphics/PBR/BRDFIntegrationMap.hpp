#pragma once

#include "../AbstractionHL.hpp"

namespace eg
{
	/**
	 * A preintegrated BRDF lookup table for use with split-sum IBL.
	 */
	class EG_API BRDFIntegrationMap
	{
	public:
		explicit BRDFIntegrationMap(uint32_t resolution = 256);
		
		TextureRef GetTexture()
		{
			return m_texture;
		}
		
		static constexpr Format FORMAT = eg::Format::R32G32_Float;
		
	private:
		Texture m_texture;
	};
}
