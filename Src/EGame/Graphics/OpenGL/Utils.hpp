#pragma once

#include "GL.hpp"
#include "../Format.hpp"
#include "../Abstraction.hpp"

namespace eg::graphics_api::gl
{
	GLenum TranslateFormat(Format format);
	GLenum TranslateDataType(DataType type);
	GLenum TranslateStencilOp(StencilOp stencilOp);
	GLenum TranslateCompareOp(CompareOp compareOp);
	GLenum Translate(BlendFunc f);
	GLenum Translate(BlendFactor f);
	GLenum Translate(Topology t);
	
	std::optional<UniformType> GetUniformType(GLenum glType);
	
	extern std::vector<GLenum> insertedBarriers;
	
	inline void ClearBarriers()
	{
		insertedBarriers.clear();
	}
	
	void MaybeInsertBarrier(GLenum barrier);
	
	template <GLenum E>
	inline void SetEnabled(bool enable)
	{
		static bool currentlyEnabled = false;
		
		if (enable && !currentlyEnabled)
			glEnable(E);
		else if (!enable && currentlyEnabled)
			glDisable(E);
		currentlyEnabled = enable;
	}
}
