#pragma once

#include "Abstraction.hpp"

namespace eg
{
class DescriptorSetWrapper
{
public:
	struct TextureBinding
	{
		TextureViewHandle textureView;
		SamplerHandle sampler;
	};

	struct BufferBinding
	{
		BufferHandle buffer;
		uint64_t offset;
		uint64_t range;
	};

	struct BindCallbacks
	{
		void (*bindBuffer)(uint32_t binding, const BufferBinding& buffer);
		void (*bindTexture)(uint32_t binding, const TextureBinding& texture);
	};

	static DescriptorSetWrapper* Allocate(uint32_t maxBindingPlusOne)
	{
		void* memory = std::calloc(1, sizeof(DescriptorSetWrapper) + sizeof(BindingEntry) * maxBindingPlusOne);
		DescriptorSetWrapper* ds = static_cast<DescriptorSetWrapper*>(memory);
		ds->m_numBindings = maxBindingPlusOne;
		return ds;
	}

	static DescriptorSetWrapper* Unwrap(DescriptorSetHandle handle)
	{
		return reinterpret_cast<DescriptorSetWrapper*>(handle);
	}

	void Free() { std::free(this); }

	DescriptorSetHandle Wrap() { return reinterpret_cast<DescriptorSetHandle>(this); }

	void BindTexture(uint32_t binding, TextureBinding texture)
	{
		EG_ASSERT(binding < m_numBindings);
		m_bindings[binding] = { .state = BindingState::Texture, .u = { .texture = texture } };
	}
	void BindBuffer(uint32_t binding, BufferBinding buffer)
	{
		EG_ASSERT(binding < m_numBindings);
		m_bindings[binding] = { .state = BindingState::Buffer, .u = { .buffer = buffer } };
	}

	template <typename CallbackType>
	void BindDescriptorSet(CallbackType bindCallback) const
	{
		for (uint32_t i = 0; i < m_numBindings; i++)
		{
			switch (m_bindings[i].state)
			{
			case BindingState::Unbound: break;
			case BindingState::Texture: bindCallback(i, m_bindings[i].u.texture); break;
			case BindingState::Buffer: bindCallback(i, m_bindings[i].u.buffer); break;
			}
		}
	}

private:
	DescriptorSetWrapper();

	enum class BindingState
	{
		Unbound = 0,
		Texture,
		Buffer,
	};

	struct BindingEntry
	{
		BindingState state;
		union
		{
			TextureBinding texture;
			BufferBinding buffer;
		} u;
	};

	uint32_t m_numBindings;
	BindingEntry m_bindings[];
};
} // namespace eg
