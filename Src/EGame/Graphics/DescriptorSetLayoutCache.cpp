#include "DescriptorSetLayoutCache.hpp"

#include <algorithm>

namespace eg
{
bool DescriptorSetLayoutCache::DSLKey::operator==(const DSLKey& o) const
{
	return dynamicBind == o.dynamicBind &&
	       std::equal(bindings.begin(), bindings.end(), o.bindings.begin(), o.bindings.end());
}

size_t DescriptorSetLayoutCache::DSLKey::Hash() const
{
	size_t h = static_cast<size_t>(dynamicBind) | (bindings.size() << 1);
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
	std::span<const DescriptorSetBinding> bindings, bool dynamicBind)
{
	DSLKey dslKey;
	dslKey.bindings = bindings;
	dslKey.dynamicBind = dynamicBind;

	if (!std::is_sorted(bindings.begin(), bindings.end(), DescriptorSetBinding::BindingCmp()))
	{
		dslKey.CreateOwnedCopyOfBindings();
		std::sort(
			dslKey.ownedBindings.get(), dslKey.ownedBindings.get() + dslKey.bindings.size(),
			DescriptorSetBinding::BindingCmp());
		bindings = dslKey.bindings;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	// Searches for a matching descriptor set in the cache
	auto cacheIt = m_layouts.find(dslKey);
	if (cacheIt != m_layouts.end())
		return *cacheIt->second;

	std::unique_ptr<ICachedDescriptorSetLayout> layout = createLayoutCallback(bindings, dynamicBind);
	ICachedDescriptorSetLayout& ret = *layout;

	if (dslKey.ownedBindings == nullptr)
		dslKey.CreateOwnedCopyOfBindings();

	m_layouts.emplace(std::move(dslKey), std::move(layout));
	return ret;
}

void DescriptorSetLayoutCache::Clear()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_layouts.clear();
}
} // namespace eg
