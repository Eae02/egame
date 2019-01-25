#pragma once

namespace eg
{
	template <typename T, typename I>
	class Lazy
	{
	public:
		explicit Lazy(I initializer)
			: m_initializer(initializer) { }
		
		bool HasValue() const
		{
			return m_value.has_value();
		}
		
		const T& operator*()
		{
			if (!m_value.has_value())
				m_value = m_initializer();
			return *m_value;
		}
		
		const T* operator->()
		{
			return &operator*();
		}
		
	private:
		std::optional<T> m_value;
		I m_initializer;
	};
	
	template <typename I>
	inline auto MakeLazy(I initializer)
	{
		return Lazy<std::invoke_result_t<I>, I>(initializer);
	}
}
