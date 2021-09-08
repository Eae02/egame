#include "Color.hpp"

namespace eg
{
	static_assert(sizeof(Color) == 16);
	
	const Color Color::White { 1.0f, 1.0f, 1.0f, 1.0f };
	const Color Color::Black { 0.0f, 0.0f, 0.0f, 1.0f };
}
