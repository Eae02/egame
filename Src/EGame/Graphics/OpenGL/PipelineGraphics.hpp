#pragma once

namespace eg::graphics_api::gl
{
extern bool viewportOutOfDate;
extern bool scissorOutOfDate;

void InitScissorTest();
bool IsDepthWriteEnabled();
} // namespace eg::graphics_api::gl
