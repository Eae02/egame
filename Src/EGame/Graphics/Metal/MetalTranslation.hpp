#pragma once

#include <Metal/Metal.hpp>

#include "../Abstraction.hpp"
#include "../Format.hpp"

namespace eg::graphics_api::mtl
{
extern MTL::PixelFormat defaultColorPixelFormat;
extern MTL::PixelFormat defaultDepthPixelFormat;

MTL::PixelFormat TranslatePixelFormat(Format format);
MTL::VertexFormat TranslateVertexFormat(Format format);

MTL::CompareFunction TranslateCompareOp(CompareOp compareOp);

MTL::BlendOperation TranslateBlendFunc(BlendFunc blendFunc);
MTL::BlendFactor TranslateBlendFactor(BlendFactor blendFactor);

MTL::CullMode TranslateCullMode(CullMode cullMode);
} // namespace eg::graphics_api::mtl
