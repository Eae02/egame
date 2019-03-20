#pragma once

#include "../Format.hpp"
#include "../Abstraction.hpp"

#include <GL/gl3w.h>

namespace eg::graphics_api::gl
{
	GLenum TranslateFormat(Format format);
	GLenum TranslateDataType(DataType type);
	GLenum TranslateCompareOp(CompareOp compareOp);
	
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
