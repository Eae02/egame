#pragma once

#include "GL.hpp"
#include "../Format.hpp"
#include "../Abstraction.hpp"

#include <optional>

namespace eg::graphics_api::gl
{
	GLenum TranslateFormatForTexture(Format format, bool returnZeroOnFailure = false);
	GLenum TranslateDataType(DataType type);
	GLenum TranslateStencilOp(StencilOp stencilOp);
	GLenum TranslateCompareOp(CompareOp compareOp);
	GLenum Translate(BlendFunc f);
	GLenum Translate(BlendFactor f);
	GLenum Translate(Topology t);
	
	enum class GLVertexAttribMode
	{
		Other,
		Norm,
		Int,
	};
	
	struct GLVertexAttribFormat
	{
		GLint size;
		GLenum type;
		GLVertexAttribMode mode;
	};
	GLVertexAttribFormat TranslateFormatForVertexAttribute(Format format, bool returnZeroOnFailure = false);
	
	std::optional<UniformType> GetUniformType(GLenum glType);
	
	enum class GLVendor
	{
		Unknown,
		Nvidia,
		Intel
	};
	
	extern std::string rendererName;
	extern std::string vendorName;
	extern GLVendor glVendor;
	
#ifdef EG_GLES
	constexpr bool useGLESPath = true;
	
	struct GLESFormatSupport
	{
		bool floatColorBuffer;
		bool floatLinearFiltering;
		bool floatBlend;
		bool compressedS3TC;
		bool compressedS3TCSRGB;
	};
	
	extern GLESFormatSupport glesFormatSupport;
#else
	extern bool useGLESPath;
#endif
	
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
	
	inline int GetIntegerLimit(GLenum name)
	{
		int res;
		glGetIntegerv(name, &res);
		return res;
	}
}
