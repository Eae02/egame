#include "Model.hpp"
#include "GraphicsLoadContext.hpp"

#include <algorithm>

namespace eg
{
Model::Model(ModelCreateArgs args)
	: m_numVertices(args.numVertices), m_meshes(std::move(args.meshes)), m_vertexFormat(args.vertexFormat),
	  m_materialNames(std::move(args.materialNames)), m_animations(std::move(args.animations))
{
	EG_ASSERT(m_numVertices > 0);

	std::sort(m_animations.begin(), m_animations.end(), AnimationNameCompare());

	m_numVertexStreams = UnsignedNarrow<uint32_t>(m_vertexFormat.streamsBytesPerVertex.size());

	if (std::holds_alternative<std::span<const uint16_t>>(args.indices))
		m_indexType = IndexType::UInt16;
	else
		m_indexType = IndexType::UInt32;

	// Calculates vertex stream offsets
	uint32_t nextVertexStreamOffset = 0;
	for (size_t s = 0; s < m_vertexFormat.streamsBytesPerVertex.size(); s++)
	{
		m_vertexStreamOffsets.at(s) = nextVertexStreamOffset;
		nextVertexStreamOffset += m_vertexFormat.streamsBytesPerVertex[s] * args.numVertices;
	}
	EG_ASSERT(nextVertexStreamOffset <= args.vertexData.size_bytes());

	// Validates attribute ranges
	for (const ModelVertexAttribute& attribute : args.vertexFormat.attributes)
	{
		uint64_t bytesPerVertex = args.vertexFormat.streamsBytesPerVertex[attribute.streamIndex];
		uint64_t verticesEnd = m_vertexStreamOffsets.at(attribute.streamIndex) + attribute.offset +
		                       bytesPerVertex * (m_numVertices - 1) + GetVertexAttributeByteWidth(attribute.type);
		EG_ASSERT(verticesEnd <= args.vertexData.size_bytes());
	}

	std::span<const char> indexData = std::visit(
		[&]<typename I>(std::span<const I> indices)
		{
			m_numIndices = indices.size();
			return std::span(reinterpret_cast<const char*>(indices.data()), indices.size_bytes());
		},
		args.indices);

	if (HasFlag(args.accessFlags, ModelAccessFlags::CPU))
	{
		m_dataForCPUAccess = DataForCPUAccess{
			.indexData = std::make_unique<char[]>(indexData.size()),
			.vertexData = std::make_unique<char[]>(args.vertexData.size()),
		};
		std::memcpy(m_dataForCPUAccess->vertexData.get(), args.vertexData.data(), args.vertexData.size());
		std::memcpy(m_dataForCPUAccess->indexData.get(), indexData.data(), indexData.size());
	}

	// Checks that all meshes refer to valid data
	for (const MeshDescriptor& mesh : m_meshes)
	{
		EG_ASSERT(mesh.firstVertex + mesh.numVertices <= m_numVertices);
		EG_ASSERT(mesh.firstIndex + mesh.numIndices <= m_numIndices);
		if (mesh.materialIndex.has_value())
			EG_ASSERT(*mesh.materialIndex < m_materialNames.size());
	}

	// Uploads vertices and indices
	const uint64_t vertexBytes = args.vertexData.size();
	const uint64_t indexBytes = indexData.size();
	const uint64_t totalBytesToUpload = vertexBytes + indexBytes;
	if (HasFlag(args.accessFlags, ModelAccessFlags::GPU) && totalBytesToUpload != 0)
	{
		GraphicsLoadContext* graphicsLoadContext =
			args.graphicsLoadContext ? args.graphicsLoadContext : &GraphicsLoadContext::Direct;

		auto stagingBuffer = graphicsLoadContext->AllocateStagingBuffer(totalBytesToUpload);

		std::copy_n(args.vertexData.begin(), vertexBytes, stagingBuffer.memory.begin());
		std::copy_n(indexData.begin(), indexData.size(), stagingBuffer.memory.begin() + vertexBytes);

		m_buffers = std::make_unique<Buffers>();

		for (uint32_t s = 0; s < m_numVertexStreams; s++)
			m_buffers->descriptor.vertexStreamOffsets[s] = m_vertexStreamOffsets[s];
		m_buffers->descriptor.indexType = m_indexType;

		graphicsLoadContext->OnGraphicsThread(
			[stagingBuffer, vertexBytes, indexBytes, buffers = m_buffers.get()](CommandContext& cc)
			{
				buffers->vertexBuffer = Buffer(
					BufferFlags::VertexBuffer | BufferFlags::CopyDst | BufferFlags::ManualBarrier, vertexBytes,
					nullptr);
				buffers->indexBuffer = Buffer(
					BufferFlags::IndexBuffer | BufferFlags::CopyDst | BufferFlags::ManualBarrier, indexBytes, nullptr);

				buffers->descriptor.vertexBuffer = buffers->vertexBuffer;
				buffers->descriptor.indexBuffer = buffers->indexBuffer;

				stagingBuffer.Flush();

				cc.CopyBuffer(stagingBuffer.buffer, buffers->vertexBuffer, stagingBuffer.bufferOffset, 0, vertexBytes);
				cc.CopyBuffer(
					stagingBuffer.buffer, buffers->indexBuffer, stagingBuffer.bufferOffset + vertexBytes, 0,
					indexBytes);

				cc.Barrier(
					buffers->vertexBuffer,
					BufferBarrier{ .oldUsage = eg::BufferUsage::CopyDst, .newUsage = eg::BufferUsage::VertexBuffer });

				cc.Barrier(
					buffers->indexBuffer,
					BufferBarrier{ .oldUsage = eg::BufferUsage::CopyDst, .newUsage = eg::BufferUsage::IndexBuffer });
			});
	}
}

std::variant<std::span<const uint32_t>, std::span<const uint16_t>> Model::GetIndices() const
{
	EG_ASSERT(m_dataForCPUAccess.has_value());
	switch (m_indexType)
	{
	case IndexType::UInt32:
		return std::span(reinterpret_cast<const uint32_t*>(m_dataForCPUAccess->indexData.get()), m_numIndices);
	case IndexType::UInt16:
		return std::span(reinterpret_cast<const uint16_t*>(m_dataForCPUAccess->indexData.get()), m_numIndices);
	}
	EG_UNREACHABLE
}

std::variant<std::span<const uint32_t>, std::span<const uint16_t>> Model::GetMeshIndices(size_t meshIndex) const
{
	const MeshDescriptor& mesh = GetMesh(meshIndex);
	return std::visit(
		[&](auto indices) -> std::variant<std::span<const uint32_t>, std::span<const uint16_t>>
		{ return indices.subspan(mesh.firstIndex, mesh.numIndices); },
		GetIndices());
}

std::optional<std::pair<uint32_t, uint32_t>> Model::GetVertexAttributeOffsetAndStride(
	ModelVertexAttributeType attributeType, uint32_t typeIndex) const
{
	std::optional<ModelVertexAttribute> attrib = m_vertexFormat.FindAttribute(attributeType, typeIndex);
	if (!attrib.has_value() || attrib->streamIndex >= m_numVertexStreams)
		return std::nullopt;
	return std::make_pair(
		m_vertexStreamOffsets[attrib->streamIndex] + attrib->offset,
		m_vertexFormat.streamsBytesPerVertex[attrib->streamIndex]);
}

std::optional<std::pair<const void*, uint32_t>> Model::GetMeshVertexAttributePtrAndStride(
	size_t meshIndex, ModelVertexAttributeType attributeType, uint32_t typeIndex) const
{
	if (!m_dataForCPUAccess.has_value())
		return std::nullopt;

	auto offsetAndStride = GetVertexAttributeOffsetAndStride(attributeType, typeIndex);
	if (!offsetAndStride.has_value())
		return std::nullopt;
	auto [offset, stride] = *offsetAndStride;
	const char* dataPtr = m_dataForCPUAccess->vertexData.get() + offset + stride * GetMesh(meshIndex).firstVertex;
	return std::make_pair(dataPtr, stride);
}

int Model::GetMeshIndex(std::string_view name) const
{
	for (int i = 0; i < ToInt(m_meshes.size()); i++)
	{
		if (m_meshes[i].name == name)
			return i;
	}
	return -1;
}

int Model::RequireMeshIndex(std::string_view name) const
{
	int idx = GetMeshIndex(name);
	if (idx != -1)
		return idx;
	std::cerr << "Mesh not found: '" << name << "', the model has the following meshes:\n";
	for (const MeshDescriptor& mesh : m_meshes)
	{
		std::cerr << " * " << mesh.name << "\n";
	}
	std::abort();
}

int Model::GetMaterialIndex(std::string_view name) const
{
	for (int i = 0; i < ToInt(m_materialNames.size()); i++)
	{
		if (m_materialNames[i] == name)
			return i;
	}
	return -1;
}

int Model::RequireMaterialIndex(std::string_view name) const
{
	int idx = GetMaterialIndex(name);
	if (idx != -1)
		return idx;
	std::cerr << "Material not found: '" << name << "', the model has the following materials:\n";
	for (const std::string& material : m_materialNames)
	{
		std::cerr << " * " << material << "\n";
	}
	std::abort();
}

const Animation* Model::FindAnimation(std::string_view name) const
{
	auto it = std::lower_bound(m_animations.begin(), m_animations.end(), name, AnimationNameCompare());
	if (it == m_animations.end() || it->name != name)
		return nullptr;
	return &*it;
}

std::optional<CollisionMesh> Model::MakeCollisionMesh(size_t meshIndex) const
{
	auto ptrAndStride = GetMeshVertexAttributePtrAndStride(meshIndex, ModelVertexAttributeType::Position_F32, 0);
	if (!ptrAndStride.has_value())
		return std::nullopt;

	CollisionMeshCreateArgs createArgs = {
		.numVertices = GetMesh(meshIndex).numVertices,
		.positionDataPtr = ptrAndStride->first,
		.positionDataStride = ptrAndStride->second,
		.indices = GetMeshIndices(meshIndex),
	};
	return CollisionMesh(createArgs);
}
std::optional<CollisionMesh> Model::MakeCollisionMesh() const
{
	if (m_meshes.size() == 0)
		return CollisionMesh();
	if (m_meshes.size() == 1)
		return MakeCollisionMesh(0);
	std::vector<CollisionMesh> meshes(m_meshes.size());
	for (size_t i = 0; i < meshes.size(); i++)
	{
		auto mesh = MakeCollisionMesh(i);
		if (!mesh.has_value())
			return std::nullopt;
		meshes[i] = std::move(*mesh);
	}
	return CollisionMesh::Join(meshes);
}

void MeshBuffersDescriptor::Bind(CommandContext& cmdCtx, uint32_t enabledBindingsMask) const
{
	// Iterates bits set in enabledBindingsMask
	while (enabledBindingsMask != 0)
	{
		uint32_t t = enabledBindingsMask & -enabledBindingsMask;
		uint32_t binding = __builtin_ctzl(enabledBindingsMask);

		if (!vertexStreamOffsets[binding].has_value()) [[unlikely]]
		{
			// clang-format off
			EG_PANIC("binding with index " << binding << " was specified as enabled but not set in the MeshBuffersDescriptor");
			// clang-format on
		}
		cmdCtx.BindVertexBuffer(binding, vertexBuffer, *vertexStreamOffsets[binding]);

		enabledBindingsMask ^= t;
	}

	if (indexBuffer.handle != nullptr) [[likely]]
	{
		cmdCtx.BindIndexBuffer(indexType, indexBuffer, 0);
	}
}
} // namespace eg
