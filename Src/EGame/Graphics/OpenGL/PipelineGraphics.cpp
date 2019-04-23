#include "OpenGL.hpp"
#include "Utils.hpp"
#include "Pipeline.hpp"
#include "Translation.hpp"
#include "OpenGLBuffer.hpp"
#include "OpenGLTexture.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"
#include "../../Span.hpp"
#include "../../Log.hpp"

#include <atomic>
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

namespace eg::graphics_api::gl
{
	struct BlendState
	{
		bool enabled;
		GLenum colorFunc;
		GLenum alphaFunc;
		GLenum srcColorFactor;
		GLenum srcAlphaFactor;
		GLenum dstColorFactor;
		GLenum dstAlphaFactor;
	};
	
	struct GraphicsPipeline : AbstractPipeline
	{
		uint32_t numShaderModules;
		GLuint shaderModules[5];
		GLuint vertexArray;
		bool enableFaceCull;
		GLenum frontFace;
		GLenum cullFace;
		GLenum depthFunc;
		GLenum topology;
		GLint patchSize;
		uint32_t numClipDistances;
		float minSampleShading;
		bool enableScissorTest;
		bool enableDepthTest;
		bool enableDepthWrite;
		BlendState blend[8];
		ColorWriteMask colorWriteMasks[8];
		uint32_t maxVertexBinding;
		VertexBinding vertexBindings[MAX_VERTEX_BINDINGS];
		
		void Free() override;
		
		void Bind() override;
	};
	
	static ObjectPool<GraphicsPipeline> gfxPipelinePool;
	
	//Indices must match ShaderStage
	static const GLenum ShaderTypes[] =
	{
		GL_VERTEX_SHADER,
		GL_FRAGMENT_SHADER,
		GL_GEOMETRY_SHADER,
		GL_TESS_CONTROL_SHADER,
		GL_TESS_EVALUATION_SHADER,
		GL_COMPUTE_SHADER
	};
	
	static const char* ShaderSuffixes[] = 
	{
		" [VS]",
		" [FS]",
		" [GS]",
		" [TCS]",
		" [TES]",
		" [CS]"
	};
	
	PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
	{
		GraphicsPipeline* pipeline = gfxPipelinePool.New();
		
		pipeline->numShaderModules = 0;
		pipeline->numClipDistances = createInfo.numClipDistances;
		
		pipeline->minSampleShading = createInfo.enableSampleShading ? createInfo.minSampleShading : 0.0f;
		
		spirv_cross::CompilerGLSL* spvCompilers[5];
		
		//Attaches shaders to the pipeline's program
		auto MaybeAddStage = [&] (const ShaderStageInfo& stageInfo, ShaderStage expectedStage)
		{
			if (stageInfo.shaderModule == nullptr)
				return;
			
			ShaderModule* module = UnwrapShaderModule(stageInfo.shaderModule);
			if (expectedStage != module->stage)
			{
				EG_PANIC("Shader stage mismatch")
			}
			spvCompilers[pipeline->numShaderModules] = &module->spvCompiler;
			
			SetSpecializationConstants(stageInfo);
			
			GLuint shader = glCreateShader(ShaderTypes[(int)expectedStage]);
			pipeline->shaderModules[pipeline->numShaderModules] = shader;
			pipeline->numShaderModules++;
			
			if (createInfo.label != nullptr)
			{
				std::string shaderLabel = Concat({ createInfo.label, ShaderSuffixes[(int)expectedStage] });
				glObjectLabel(GL_SHADER, shader, -1, shaderLabel.c_str());
			}
		};
		MaybeAddStage(createInfo.vertexShader, eg::ShaderStage::Vertex);
		MaybeAddStage(createInfo.fragmentShader, eg::ShaderStage::Fragment);
		MaybeAddStage(createInfo.geometryShader, eg::ShaderStage::Geometry);
		MaybeAddStage(createInfo.tessControlShader, eg::ShaderStage::TessControl);
		MaybeAddStage(createInfo.tessEvaluationShader, eg::ShaderStage::TessEvaluation);
		
		pipeline->Initialize(pipeline->numShaderModules, spvCompilers, pipeline->shaderModules);
		
		if (createInfo.label != nullptr)
		{
			glObjectLabel(GL_PROGRAM, pipeline->program, -1, createInfo.label);
		}
		
		glCreateVertexArrays(1, &pipeline->vertexArray);
		for (uint32_t i = 0; i < MAX_VERTEX_ATTRIBUTES; i++)
		{
			uint32_t binding = createInfo.vertexAttributes[i].binding;
			if (binding == UINT32_MAX)
				continue;
			
			glEnableVertexArrayAttrib(pipeline->vertexArray, i);
			glVertexArrayAttribBinding(pipeline->vertexArray, i, binding);
			
			const static DataType intDataTypes[] = {
				DataType::UInt8, DataType::UInt16, DataType::UInt32,
				DataType::SInt8, DataType::SInt16, DataType::SInt32
			};
			
			const static DataType normDataTypes[] = {
				DataType::UInt8Norm, DataType::UInt16Norm, DataType::SInt8Norm, DataType::SInt16Norm
			};
			
			DataType type = createInfo.vertexAttributes[i].type;
			GLenum glType = TranslateDataType(type);
			
			if (eg::Contains(intDataTypes, type))
			{
				glVertexArrayAttribIFormat(pipeline->vertexArray, i, createInfo.vertexAttributes[i].components,
					glType, createInfo.vertexAttributes[i].offset);
			}
			else
			{
				auto normalized = static_cast<GLboolean>(eg::Contains(normDataTypes, type));
				glVertexArrayAttribFormat(pipeline->vertexArray, i, createInfo.vertexAttributes[i].components,
					glType, normalized, createInfo.vertexAttributes[i].offset);
			}
		}
		
		pipeline->maxVertexBinding = 0;
		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
		{
			pipeline->vertexBindings[i] = createInfo.vertexBindings[i];
			if (createInfo.vertexBindings[i].stride == UINT32_MAX)
				continue;
			glVertexArrayBindingDivisor(pipeline->vertexArray, i, (GLuint)createInfo.vertexBindings[i].inputRate);
			pipeline->maxVertexBinding = i + 1;
		}
		
		pipeline->enableScissorTest = createInfo.enableScissorTest;
		pipeline->enableDepthTest = createInfo.enableDepthTest;
		pipeline->enableDepthWrite = createInfo.enableDepthWrite;
		pipeline->topology = Translate(createInfo.topology);
		pipeline->patchSize = createInfo.patchControlPoints;
		
		switch (createInfo.cullMode)
		{
		case CullMode::None:
			pipeline->enableFaceCull = false;
			pipeline->cullFace = GL_BACK;
			break;
		case CullMode::Front:
			pipeline->enableFaceCull = true;
			pipeline->cullFace = GL_FRONT;
			break;
		case CullMode::Back:
			pipeline->enableFaceCull = true;
			pipeline->cullFace = GL_BACK;
			break;
		}
		
		pipeline->depthFunc = TranslateCompareOp(createInfo.depthCompare);
		
		for (int i = 0; i < 8; i++)
		{
			bool enabled = pipeline->blend[i].enabled = createInfo.blendStates[i].enabled;
			if (enabled)
			{
				pipeline->blend[i].colorFunc = Translate(createInfo.blendStates[i].colorFunc);
				pipeline->blend[i].alphaFunc = Translate(createInfo.blendStates[i].alphaFunc);
				pipeline->blend[i].srcColorFactor = Translate(createInfo.blendStates[i].srcColorFactor);
				pipeline->blend[i].srcAlphaFactor = Translate(createInfo.blendStates[i].srcAlphaFactor);
				pipeline->blend[i].dstColorFactor = Translate(createInfo.blendStates[i].dstColorFactor);
				pipeline->blend[i].dstAlphaFactor = Translate(createInfo.blendStates[i].dstAlphaFactor);
			}
		}
		
		pipeline->frontFace = createInfo.frontFaceCCW ? GL_CCW : GL_CW;
		
		return reinterpret_cast<PipelineHandle>(pipeline);
	}
	
	void PipelineFramebufferFormatHint(PipelineHandle handle, const FramebufferFormatHint& hint) { }
	
	void GraphicsPipeline::Free()
	{
		for (uint32_t i = 0; i < numShaderModules; i++)
			glDeleteShader(shaderModules[i]);
		gfxPipelinePool.Free(this);
	}
	
	static struct
	{
		GLenum frontFace = GL_CCW;
		GLenum cullFace = GL_BACK;
		GLenum depthFunc = GL_LESS;
		GLint patchSize = 0;
		uint32_t numClipDistances = 0;
		uint32_t numCullDistances = 0;
		float minSampleShading = 0;
		bool enableDepthWrite = true;
		bool blendEnabled[8] = { };
		ColorWriteMask colorWriteMasks[8] = { };
	} curState;
	
	static bool updateVAOBindings = false;
	
	static float currentViewport[4];
	static int currentScissor[4];
	bool viewportOutOfDate;
	bool scissorOutOfDate;
	
	bool IsDepthWriteEnabled()
	{
		return curState.enableDepthWrite;
	}
	
	void SetViewport(CommandContextHandle, float x, float y, float w, float h)
	{
		if (!FEqual(currentViewport[0], x) || !FEqual(currentViewport[1], y) ||
		    !FEqual(currentViewport[2], w) || !FEqual(currentViewport[3], h))
		{
			currentViewport[0] = x;
			currentViewport[1] = y;
			currentViewport[2] = w;
			currentViewport[3] = h;
			viewportOutOfDate = true;
		}
	}
	
	void SetScissor(CommandContextHandle, int x, int y, int w, int h)
	{
		if (currentScissor[0] != x || currentScissor[1] != y ||
		    currentScissor[2] != w || currentScissor[3] != h)
		{
			currentScissor[0] = x;
			currentScissor[1] = y;
			currentScissor[2] = w;
			currentScissor[3] = h;
			scissorOutOfDate = true;
		}
	}
	
	inline bool IsScissorTestEnabled()
	{
		return static_cast<const GraphicsPipeline*>(currentPipeline)->enableScissorTest;
	}
	
	void InitScissorTest()
	{
		if (currentPipeline != nullptr)
		{
			SetEnabled<GL_SCISSOR_TEST>(IsScissorTestEnabled());
		}
	}
	
	inline void CommitViewportAndScissor()
	{
		if (currentPipeline == nullptr)
			return;
		
		if (viewportOutOfDate)
		{
			glViewportArrayv(0, 1, currentViewport);
			viewportOutOfDate = false;
		}
		
		if (IsScissorTestEnabled() && scissorOutOfDate)
		{
			glScissorArrayv(0, 1, currentScissor);
			scissorOutOfDate = false;
		}
	}
	
	void GraphicsPipeline::Bind()
	{
		glBindVertexArray(vertexArray);
		
		if (curState.frontFace != frontFace)
			glFrontFace(curState.frontFace = frontFace);
		if (curState.cullFace != cullFace)
			glCullFace(curState.cullFace = cullFace);
		if (enableDepthTest && curState.depthFunc != depthFunc)
			glDepthFunc(curState.depthFunc = depthFunc);
		
		SetEnabled<GL_CULL_FACE>(enableFaceCull);
		SetEnabled<GL_DEPTH_TEST>(enableDepthTest);
		
		while (numClipDistances > curState.numClipDistances)
		{
			glEnable(GL_CLIP_DISTANCE0 + curState.numClipDistances);
			curState.numClipDistances++;
		}
		while (curState.numClipDistances > numClipDistances)
		{
			curState.numClipDistances--;
			glDisable(GL_CLIP_DISTANCE0 + curState.numClipDistances);
		}
		
		InitScissorTest();
		
		if (minSampleShading != curState.minSampleShading)
		{
			glMinSampleShading(minSampleShading);
			curState.minSampleShading = minSampleShading;
		}
		
		if (patchSize != 0 && curState.patchSize != patchSize)
		{
			glPatchParameteri(GL_PATCH_VERTICES, patchSize);
			curState.patchSize = patchSize;
		}
		
		if (curState.enableDepthWrite != enableDepthWrite)
		{
			glDepthMask(static_cast<GLboolean>(enableDepthWrite));
			curState.enableDepthWrite = enableDepthWrite;
		}
		
		for (GLuint i = 0; i < 8; i++)
		{
			if (curState.colorWriteMasks[i] != colorWriteMasks[i])
			{
				glColorMaski(i, HasFlag(colorWriteMasks[i], ColorWriteMask::R),
				             HasFlag(colorWriteMasks[i], ColorWriteMask::G),
				             HasFlag(colorWriteMasks[i], ColorWriteMask::B),
				             HasFlag(colorWriteMasks[i], ColorWriteMask::A));
				curState.colorWriteMasks[i] = colorWriteMasks[i];
			}
			if (curState.blendEnabled[i] != blend[i].enabled)
			{
				if (blend[i].enabled)
					glEnablei(GL_BLEND, i);
				else
					glDisablei(GL_BLEND, i);
				curState.blendEnabled[i] = blend[i].enabled;
			}
			if (blend[i].enabled)
			{
				glBlendEquationSeparatei(i, blend[i].colorFunc, blend[i].alphaFunc);
				glBlendFuncSeparatei(i, blend[i].srcColorFactor, blend[i].dstColorFactor,
					blend[i].srcAlphaFactor, blend[i].dstAlphaFactor);
			}
		}
		
		updateVAOBindings = true;
	}
	
	static IndexType currentIndexType;
	static uint32_t indexBufferOffset;
	static GLuint indexBuffer;
	static std::pair<GLuint, uint32_t> vertexBuffers[MAX_VERTEX_BINDINGS];
	
	inline void MaybeUpdateVAO()
	{
		if (!updateVAOBindings)
			return;
		
		const GraphicsPipeline* graphicsPipeline = static_cast<const GraphicsPipeline*>(currentPipeline);
		for (uint32_t i = 0; i < graphicsPipeline->maxVertexBinding; i++)
		{
			if (graphicsPipeline->vertexBindings[i].stride != UINT32_MAX)
			{
				glBindVertexBuffer(i, vertexBuffers[i].first, vertexBuffers[i].second,
				                   graphicsPipeline->vertexBindings[i].stride);
			}
		}
		
		glVertexArrayElementBuffer(graphicsPipeline->vertexArray, indexBuffer);
		
		updateVAOBindings = false;
	}
	
	void BindVertexBuffer(CommandContextHandle, uint32_t binding, BufferHandle buffer, uint32_t offset)
	{
		vertexBuffers[binding] = std::make_pair(reinterpret_cast<Buffer*>(buffer)->buffer, offset);
		updateVAOBindings = true;
	}
	
	void BindIndexBuffer(CommandContextHandle, IndexType type, BufferHandle buffer, uint32_t offset)
	{
		currentIndexType = type;
		indexBuffer = reinterpret_cast<Buffer*>(buffer)->buffer;
		indexBufferOffset = offset;
		updateVAOBindings = true;
	}
	
	void Draw(CommandContextHandle, uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances)
	{
		CommitViewportAndScissor();
		MaybeUpdateVAO();
		
		glDrawArraysInstancedBaseInstance(static_cast<const GraphicsPipeline*>(currentPipeline)->topology,
			firstVertex, numVertices, numInstances, firstInstance);
		
		ClearBarriers();
	}
	
	void DrawIndexed(CommandContextHandle, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex,
		uint32_t firstInstance, uint32_t numInstances)
	{
		CommitViewportAndScissor();
		MaybeUpdateVAO();
		
		uintptr_t indexOffset = indexBufferOffset + firstIndex * 2;
		GLenum indexType = GL_UNSIGNED_SHORT;
		
		if (currentIndexType == IndexType::UInt32)
		{
			indexType = GL_UNSIGNED_INT;
			indexOffset += firstIndex * 2;
		}
		
		glDrawElementsInstancedBaseVertexBaseInstance(static_cast<const GraphicsPipeline*>(currentPipeline)->topology,
			numIndices, indexType, (void*)indexOffset, numInstances, firstVertex, firstInstance);
		
		ClearBarriers();
	}
}
