#pragma once

#include "../API.hpp"

namespace eg::renderdoc
{
	void Init();
	
	EG_API bool IsPresent();
	EG_API void CaptureNextFrame();
	EG_API void StartCapture();
	EG_API void EndCapture();
}
