#pragma once

#include "../Hash.hpp"
#include "Abstraction.hpp"

#include <memory>
#include <unordered_map>

namespace eg
{
struct ICachedDescriptorSetLayout
{
	virtual ~ICachedDescriptorSetLayout() {}
};

class DescriptorSetLayoutCache
{
public:
	using CreateLayoutCallback = std::unique_ptr<ICachedDescriptorSetLayout> (*)(
		std::span<const DescriptorSetBinding> bindings, bool dynamicBind);

	using NormalizeBindingCallback = DescriptorSetBinding (*)(const DescriptorSetBinding&);

	explicit DescriptorSetLayoutCache(CreateLayoutCallback _createLayoutCallback = nullptr)
		: createLayoutCallback(_createLayoutCallback)
	{
	}

	template <typename T>
	static std::unique_ptr<ICachedDescriptorSetLayout> ConstructorCreateLayoutCallback(
		std::span<const DescriptorSetBinding> bindings, bool dynamicBind)
	{
		return std::make_unique<T>(bindings, dynamicBind);
	}

	CreateLayoutCallback createLayoutCallback;

	ICachedDescriptorSetLayout& Get(std::span<const DescriptorSetBinding> bindings, bool dynamicBind);

	bool IsEmpty() const { return m_layouts.empty(); }
	void Clear() { m_layouts.clear(); }

private:
	struct DSLKey
	{
		bool dynamicBind;
		std::span<const DescriptorSetBinding> bindings;
		std::unique_ptr<DescriptorSetBinding[]> ownedBindings;

		bool operator==(const DSLKey& o) const;

		size_t Hash() const;

		void CreateOwnedCopyOfBindings();
	};

	std::unordered_map<DSLKey, std::unique_ptr<ICachedDescriptorSetLayout>, MemberFunctionHash<DSLKey>> m_layouts;
};
} // namespace eg
