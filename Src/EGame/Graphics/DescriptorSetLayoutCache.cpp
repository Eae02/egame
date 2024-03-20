#include "DescriptorSetLayoutCache.hpp"

#include <algorithm>

namespace eg
{
bool DescriptorSetLayoutCache::DSLKey::operator==(const DSLKey& o) const
{
	return bindMode == o.bindMode && std::equal(bindings.begin(), bindings.end(), o.bindings.begin(), o.bindings.end());
}

size_t DescriptorSetLayoutCache::DSLKey::Hash() const
{
	size_t h = static_cast<size_t>(bindMode) | (bindings.size() << 1);
	for (const DescriptorSetBinding& binding : bindings)
	{
		HashAppend(h, binding.Hash());
	}
	return h;
}

void DescriptorSetLayoutCache::DSLKey::CreateOwnedCopyOfBindings()
{
	ownedBindings = std::make_unique<DescriptorSetBinding[]>(bindings.size());
	std::copy(bindings.begin(), bindings.end(), ownedBindings.get());
	bindings = { ownedBindings.get(), bindings.size() };
}

ICachedDescriptorSetLayout& DescriptorSetLayoutCache::Get(
	std::span<const DescriptorSetBinding> bindings, BindMode bindMode)
{
	DSLKey dslKey;
	dslKey.bindings = bindings;
	dslKey.bindMode = bindMode;

	if (!std::is_sorted(bindings.begin(), bindings.end(), DescriptorSetBinding::BindingCmp()))
	{
		dslKey.CreateOwnedCopyOfBindings();
		std::sort(
			dslKey.ownedBindings.get(), dslKey.ownedBindings.get() + dslKey.bindings.size(),
			DescriptorSetBinding::BindingCmp());
		bindings = dslKey.bindings;
	}

	// Searches for a matching descriptor set in the cache
	auto cacheIt = m_layouts.find(dslKey);
	if (cacheIt != m_layouts.end())
		return *cacheIt->second;

	std::unique_ptr<ICachedDescriptorSetLayout> layout = createLayoutCallback(bindings, bindMode);
	ICachedDescriptorSetLayout& ret = *layout;

	dslKey.ownedBindings = std::make_unique<DescriptorSetBinding[]>(bindings.size());
	std::copy(bindings.begin(), bindings.end(), dslKey.ownedBindings.get());
	dslKey.bindings = { dslKey.ownedBindings.get(), bindings.size() };

	m_layouts.emplace(std::move(dslKey), std::move(layout));
	return ret;
}
} // namespace eg
