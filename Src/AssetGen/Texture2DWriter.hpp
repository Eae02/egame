#pragma once

#include "../EGame/Graphics/Format.hpp"
#include "../EGame/Utils.hpp"
#include "../EGame/Assets/Texture2DLoader.hpp"

#include <span>
#include <yaml-cpp/yaml.h>

namespace eg::asset_gen
{
	class Texture2DWriter
	{
	public:
		Texture2DWriter() = default;
		
		void ParseYAMLSettings(const YAML::Node& node);
		
		bool AddLayer(std::istream& imageStream);
		
		[[nodiscard]]
		bool Write(std::ostream& stream) const;
		
		void SetIsArrayTexture(bool isArrayTexture)
		{
			m_isArrayTexture = isArrayTexture;
		}
		
		void SetIsCubeMap(bool isCubeMap)
		{
			m_isCubeMap = isCubeMap;
		}
		
		void SetIs3D(bool is3D)
		{
			m_is3D = is3D;
		}
		
	private:
		void ProcessMipLevel(const uint8_t* imageData, int width, int height, int mode);
		
		Format m_format = Format::R8G8B8A8_UNorm;
		bool m_isSRGB = false;
		
		int m_numMipLevels = 0;
		int m_width = -1;
		int m_height = -1;
		int m_numLayers = 0;
		
		int m_mipShiftLow = 0;
		int m_mipShiftMedium = 0;
		int m_mipShiftHigh = 0;
		
		bool m_isArrayTexture = false;
		bool m_isCubeMap = false;
		bool m_is3D = false;
		bool m_dxtHighQuality = false;
		bool m_dxtDither = false;
		bool m_linearFiltering = true;
		bool m_anisotropicFiltering = true;
		bool m_useGlobalDownscale = false;
		
		std::vector<std::unique_ptr<uint8_t, FreeDel>> m_freeDelUP;
		
		std::vector<std::span<const uint8_t>> m_data;
	};
}
