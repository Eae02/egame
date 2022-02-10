#pragma once

#include "../AbstractionHL.hpp"

#include <span>
#include <memory>
#include <vector>
#include <variant>

namespace eg
{
	class EG_API BoneMatrixBuffer
	{
	public:
		struct MatrixRangeReference
		{
			uint32_t byteOffset;
			std::optional<uint32_t> matrixOffset; //will only be set if BoneMatrixBuffer::offsetAlignment is 0
		};
		
		enum class UsageMode
		{
			StorageBuffer,
			UniformBuffer
		};
		
		explicit BoneMatrixBuffer(UsageMode usageMode = UsageMode::StorageBuffer)
			: m_usageMode(usageMode) { }
		
		void Begin();
		void End();
		
#if __cpp_lib_shared_ptr_arrays == 201707L
		MatrixRangeReference AddShared(std::shared_ptr<const glm::mat4[]> matrices, uint32_t count);
		MatrixRangeReference AddShared(const class AnimationDriver& animationDriver);
#endif
		
		MatrixRangeReference AddCopy(std::span<const glm::mat4> matrices);
		
		//matrices will not be copied, so memory must be available until End is called.
		MatrixRangeReference AddNoCopy(std::span<const glm::mat4> matrices);
		
		void CreateDescriptorSet();
		DescriptorSetRef GetDescriptorSet() const { return m_descriptorSet; }
		
		BufferRef GetBuffer() const { return m_deviceBuffer; }
		uint32_t BufferVersion() const { return m_bufferVersion; }
		
		uint32_t offsetAlignment = 0;
		
	private:
		MatrixRangeReference StepPosition(uint32_t numMatrices);
		
		void UpdateDescriptorSet();
		
		struct MatrixRange
		{
#if __cpp_lib_shared_ptr_arrays == 201707L
			std::variant<const glm::mat4*, std::shared_ptr<const glm::mat4[]>, size_t> matrices;
#else
			std::variant<const glm::mat4*, size_t> matrices;
#endif
			
			uint32_t offset;
			uint32_t size;
		};
		
		uint32_t m_size = 0;
		uint32_t m_position = 0;
		
		uint32_t m_bufferVersion = 0;
		
		std::vector<MatrixRange> m_matrixRanges;
		
		Buffer m_deviceBuffer;
		Buffer m_stagingBuffer;
		char* m_stagingBufferMapping = nullptr;
		
		std::vector<glm::mat4> m_ownedMatrices;
		
		UsageMode m_usageMode;
		
		DescriptorSet m_descriptorSet;
	};
}
