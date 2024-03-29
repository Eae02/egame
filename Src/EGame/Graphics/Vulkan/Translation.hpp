#pragma once

#ifndef EG_NO_VULKAN

#include "Common.hpp"

namespace eg::graphics_api::vk
{
VkBlendOp TranslateBlendFunc(BlendFunc blendFunc);
VkBlendFactor TranslateBlendFactor(BlendFactor blendFactor);
VkCullModeFlags TranslateCullMode(CullMode mode);
VkFormat TranslateFormat(Format format);
VkCompareOp TranslateCompareOp(CompareOp op);
VkStencilOp TranslateStencilOp(StencilOp op);
VkPipelineStageFlags TranslateShaderPipelineStage(ShaderAccessFlags accessFlags);
VkShaderStageFlags TranslateShaderStage(ShaderAccessFlags accessFlags);
VkDescriptorType TranslateBindingType(BindingType bindingType);
} // namespace eg::graphics_api::vk

#endif
