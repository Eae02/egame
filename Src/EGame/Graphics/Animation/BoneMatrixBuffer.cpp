#include "BoneMatrixBuffer.hpp"
#include "AnimationDriver.hpp"

namespace eg
{
	static DescriptorSetBinding dsBinding(0, BindingType::StorageBuffer, ShaderAccessFlags::Vertex);
	
	void BoneMatrixBuffer::CreateDescriptorSet()
	{
		if (!m_descriptorSet.handle)
		{
			m_descriptorSet = DescriptorSet({ &dsBinding, 1 });
			UpdateDescriptorSet();
		}
	}
	
	void BoneMatrixBuffer::UpdateDescriptorSet()
	{
		if (m_deviceBuffer.handle && m_descriptorSet.handle)
		{
			if (m_usageMode == UsageMode::StorageBuffer)
				m_descriptorSet.BindStorageBuffer(m_deviceBuffer, 0, 0, m_size);
			else
				m_descriptorSet.BindUniformBuffer(m_deviceBuffer, 0, 0, m_size);
		}
	}
	
	void BoneMatrixBuffer::Begin()
	{
		m_position = 0;
		m_ownedMatrices.clear();
		m_matrixRanges.clear();
	}
	
#if __cpp_lib_shared_ptr_arrays == 201707L
	BoneMatrixBuffer::MatrixRangeReference BoneMatrixBuffer::AddShared(const AnimationDriver& animationDriver)
	{
		return AddShared(animationDriver.BoneMatricesSP(), animationDriver.NumBoneMatrices());
	}
	
	BoneMatrixBuffer::MatrixRangeReference BoneMatrixBuffer::AddShared(
		std::shared_ptr<const glm::mat4[]> matrices, uint32_t count)
	{
		MatrixRangeReference rangeRef = StepPosition(count);
		
		MatrixRange& matrixRange = m_matrixRanges.emplace_back();
		matrixRange.matrices = std::move(matrices);
		matrixRange.size = count * sizeof(glm::mat4);
		matrixRange.offset = rangeRef.byteOffset;
		
		return rangeRef;
	}
#endif
	
	BoneMatrixBuffer::MatrixRangeReference BoneMatrixBuffer::AddNoCopy(std::span<const glm::mat4> matrices)
	{
		MatrixRangeReference rangeRef = StepPosition(matrices.size());
		
		MatrixRange& matrixRange = m_matrixRanges.emplace_back();
		matrixRange.matrices = matrices.data();
		matrixRange.size = matrices.size_bytes();
		matrixRange.offset = rangeRef.byteOffset;
		
		return rangeRef;
	}
	
	BoneMatrixBuffer::MatrixRangeReference BoneMatrixBuffer::AddCopy(std::span<const glm::mat4> matrices)
	{
		MatrixRangeReference rangeRef = StepPosition(matrices.size());
		
		MatrixRange& matrixRange = m_matrixRanges.emplace_back();
		matrixRange.matrices = m_ownedMatrices.size();
		matrixRange.size = matrices.size_bytes();
		matrixRange.offset = rangeRef.byteOffset;
		
		m_ownedMatrices.insert(m_ownedMatrices.end(), matrices.begin(), matrices.end());
		
		return rangeRef;
	}
	
	BoneMatrixBuffer::MatrixRangeReference BoneMatrixBuffer::StepPosition(uint32_t numMatrices)
	{
		if (offsetAlignment != 0)
		{
			m_position = RoundToNextMultiple(m_position, offsetAlignment);
		}
		
		MatrixRangeReference ref;
		ref.byteOffset = m_position;
		if (offsetAlignment == 0)
			ref.matrixOffset = m_position / sizeof(glm::mat4);
		
		m_position += numMatrices * sizeof(glm::mat4);
		return ref;
	}
	
	void BoneMatrixBuffer::End()
	{
		if (m_position == 0)
			return;
		
		//Reallocates buffers if current ones are too small
		if (m_position > m_size)
		{
			m_size = RoundToNextMultiple(m_position, 16 * 1024 * sizeof(glm::mat4));
			
			eg::BufferFlags bufferFlags = eg::BufferFlags::CopyDst;
			if (m_usageMode == UsageMode::StorageBuffer)
				bufferFlags |= eg::BufferFlags::StorageBuffer;
			else
				bufferFlags |= eg::BufferFlags::UniformBuffer;
			
			m_deviceBuffer = Buffer(bufferFlags, m_size, nullptr);
			m_stagingBuffer = Buffer(BufferFlags::MapWrite | BufferFlags::CopySrc | BufferFlags::HostAllocate,
			                         m_size * MAX_CONCURRENT_FRAMES, nullptr);
			
			m_stagingBufferMapping = static_cast<char*>(m_stagingBuffer.Map(0, m_size));
			
			UpdateDescriptorSet();
			
			m_bufferVersion++;
		}
		
		//Copies data to the staging buffer
		uint64_t stagingBufferOffset = CFrameIdx() * MAX_CONCURRENT_FRAMES;
		for (const MatrixRange& range : m_matrixRanges)
		{
			char* dst = m_stagingBufferMapping + stagingBufferOffset + range.offset;
			if (auto matrices = std::get_if<const glm::mat4*>(&range.matrices))
			{
				std::memcpy(dst, *matrices, range.size);
			}
#if __cpp_lib_shared_ptr_arrays == 201707L
			else if (auto matrices = std::get_if<const std::shared_ptr<const glm::mat4[]>*>(&range.matrices))
			{
				std::memcpy(dst, (**matrices).get(), range.size);
			}
#endif
			else if (const size_t* ownedMatricesOffset = std::get_if<size_t>(&range.matrices))
			{
				std::memcpy(dst, &m_ownedMatrices[*ownedMatricesOffset], range.size);
			}
		}
		
		m_stagingBuffer.Flush(stagingBufferOffset, m_position);
		
		DC.CopyBuffer(m_stagingBuffer, m_deviceBuffer, stagingBufferOffset, 0, m_position);
		
		if (m_usageMode == UsageMode::StorageBuffer)
		{
			m_deviceBuffer.UsageHint(eg::BufferUsage::StorageBufferRead, eg::ShaderAccessFlags::Vertex);
		}
		else
		{
			m_deviceBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex);
		}
		
		m_matrixRanges.clear();
	}
}
