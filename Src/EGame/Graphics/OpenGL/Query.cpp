#include "GL.hpp"
#include "OpenGLBuffer.hpp"
#include "../Abstraction.hpp"
#include "../../Alloc/PoolAllocator.hpp"

namespace eg::graphics_api::gl
{
	struct QueryPool
	{
		uint32_t size;
		GLenum target;
		GLuint queries[1];
	};
	
	QueryPoolHandle CreateQueryPool(QueryType type, uint32_t queryCount)
	{
		void* poolMemory = std::malloc(sizeof(QueryPool) + sizeof(GLuint) * (queryCount - 1));
		QueryPool* pool = static_cast<QueryPool*>(poolMemory);
		pool->size = queryCount;
		
		glGenQueries(queryCount, pool->queries);
		
		return reinterpret_cast<QueryPoolHandle>(pool);
	}
	
	inline QueryPool* UnwrapQueryPool(QueryPoolHandle handle)
	{
		return reinterpret_cast<QueryPool*>(handle);
	}
	
	inline void CheckQueryIndex(QueryPool& pool, uint32_t index)
	{
		if (index >= pool.size)
		{
			EG_PANIC("Query index out of range")
		}
	}
	
	void DestroyQueryPool(QueryPoolHandle queryPoolHandle)
	{
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		glDeleteQueries(queryPool->size, queryPool->queries);
		std::free(queryPool);
	}
	
	template <bool CheckAvail>
	inline bool _GetQueryResults(QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries, void* data)
	{
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		CheckQueryIndex(*queryPool, firstQuery + numQueries);
		for (uint32_t i = 0; i < numQueries; i++)
		{
			uint32_t queryIndex = firstQuery + i;
			GLuint query = queryPool->queries[queryIndex];
			
			if constexpr (CheckAvail)
			{
				GLuint available;
				glGetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
				if (!available)
					return false;
			}
			
			
#ifdef __EMSCRIPTEN__
			GLuint value;
			glGetQueryObjectuiv(query, GL_QUERY_RESULT, &value);
			((uint64_t*)data)[i] = value;
#else
			glGetQueryObjectui64v(query, GL_QUERY_RESULT, (uint64_t*)data + i);
#endif
		}
		
		return true;
	}
	
	bool GetQueryResults(QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries,
	                     uint64_t dataSize, void* data)
	{
#ifdef EG_GLES
		return true;
#else
		if (dataSize < sizeof(uint64_t) * numQueries)
		{
			EG_PANIC("GetQueryResults: dataSize too small")
		}
		return _GetQueryResults<true>(queryPoolHandle, firstQuery, numQueries, data);
#endif
	}
	
	void CopyQueryResults(CommandContextHandle, QueryPoolHandle queryPoolHandle,
		uint32_t firstQuery, uint32_t numQueries, BufferHandle dstBufferHandle, uint64_t dstOffset)
	{
#ifdef EG_GLES
		static bool hasWarned = false;
		if (!hasWarned)
		{
			Log(LogLevel::Error, "gl", "CopyQueryResults is not available in GLES");
			hasWarned = true;
		}
#else
		glBindBuffer(GL_QUERY_BUFFER, UnwrapBuffer(dstBufferHandle)->buffer);
		_GetQueryResults<false>(queryPoolHandle, firstQuery, numQueries, reinterpret_cast<void*>(dstOffset));
		glBindBuffer(GL_QUERY_BUFFER, 0);
#endif
	}
	
	void WriteTimestamp(CommandContextHandle, QueryPoolHandle queryPoolHandle, uint32_t query)
	{
#ifdef EG_GLES
		static bool hasWarned = false;
		if (!hasWarned)
		{
			Log(LogLevel::Error, "gl", "WriteTimestamp is not available in GLES");
			hasWarned = true;
		}
#else
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		CheckQueryIndex(*queryPool, query);
		glQueryCounter(queryPool->queries[query], GL_TIMESTAMP);
#endif
	}
	
	void ResetQueries(CommandContextHandle, QueryPoolHandle, uint32_t, uint32_t) { }
	
	void BeginQuery(CommandContextHandle, QueryPoolHandle queryPoolHandle, uint32_t query)
	{
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		CheckQueryIndex(*queryPool, query);
		glBeginQuery(queryPool->queries[query], queryPool->target);
	}
	
	void EndQuery(CommandContextHandle, QueryPoolHandle queryPoolHandle, uint32_t query)
	{
		QueryPool* queryPool = UnwrapQueryPool(queryPoolHandle);
		CheckQueryIndex(*queryPool, query);
		glEndQuery(queryPool->queries[query]);
	}
}
