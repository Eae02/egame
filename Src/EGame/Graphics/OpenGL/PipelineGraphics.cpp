#include "PipelineGraphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../Assert.hpp"
#include "../../Log.hpp"
#include "../../MainThreadInvoke.hpp"
#include "../../String.hpp"
#include "../Graphics.hpp"
#include "Framebuffer.hpp"
#include "OpenGL.hpp"
#include "OpenGLBuffer.hpp"
#include "OpenGLTexture.hpp"
#include "Pipeline.hpp"
#include "Utils.hpp"

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

struct GLStencilState
{
	GLenum failOp;
	GLenum passOp;
	GLenum depthFailOp;
	GLenum compareOp;
	uint32_t compareMask;
	uint32_t writeMask;
	uint32_t reference;
};

inline void TranslateStencilState(GLStencilState& output, const StencilState& input)
{
	output.failOp = TranslateStencilOp(input.failOp);
	output.passOp = TranslateStencilOp(input.passOp);
	output.depthFailOp = TranslateStencilOp(input.depthFailOp);
	output.compareOp = TranslateCompareOp(input.compareOp);
	output.compareMask = input.compareMask;
	output.writeMask = input.writeMask;
	output.reference = input.reference;
}

struct VertexAttribData
{
	uint32_t attribIndex; // vertex attributes are sorted by binding, so need to store the original attribute index.
	uint32_t binding;
	uint32_t dataOffset;
	GLVertexAttribFormat format;
};

struct GraphicsPipeline : AbstractPipeline
{
	uint32_t numShaderModules;
	GLuint shaderModules[5];
	GLuint vertexArray;
	bool wireframe;
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
	bool enableStencilTest;
	GLStencilState frontStencilState;
	GLStencilState backStencilState;
	BlendState blend[8];
	float blendConstants[4];
	ColorWriteMask colorWriteMasks[8];

	VertexBinding vertexBindings[MAX_VERTEX_BINDINGS];
	uint32_t numActiveVertexAttribs;
	VertexAttribData vertexAttribs[MAX_VERTEX_ATTRIBUTES];

	void Free() override;

	void Bind() override;
};

static ObjectPool<GraphicsPipeline> gfxPipelinePool;

// Indices must match ShaderStage
static const std::array<GLenum, 6> ShaderTypes = { GL_VERTEX_SHADER,          GL_FRAGMENT_SHADER,
	                                               GL_GEOMETRY_SHADER,        GL_TESS_CONTROL_SHADER,
	                                               GL_TESS_EVALUATION_SHADER, GL_COMPUTE_SHADER };

static std::array<const char*, 6> ShaderSuffixes = { " [VS]", " [FS]", " [GS]", " [TCS]", " [TES]", " [CS]" };

PipelineHandle CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo)
{
#ifdef __EMSCRIPTEN__
	if (createInfo.numClipDistances != 0)
	{
		Log(LogLevel::Error, "gl", "Shader clip distances are not supported in WebGL");
	}
#endif

	GraphicsPipeline* pipeline = gfxPipelinePool.New();

	pipeline->isGraphicsPipeline = true;
	pipeline->numShaderModules = 0;
	pipeline->numClipDistances = createInfo.numClipDistances;

	pipeline->minSampleShading = createInfo.enableSampleShading ? createInfo.minSampleShading : 0.0f;

	std::pair<spirv_cross::CompilerGLSL*, GLuint> shaderStages[5];
	auto* spvCompilers = static_cast<spirv_cross::CompilerGLSL*>(alloca(sizeof(spirv_cross::CompilerGLSL) * 5));

	// Attaches shaders to the pipeline's program
	uint32_t currentIOGroup = 0;
	auto MaybeAddStage = [&](const ShaderStageInfo& stageInfo, ShaderStage expectedStage)
	{
		if (stageInfo.shaderModule == nullptr)
			return;

		ShaderModule* module = UnwrapShaderModule(stageInfo.shaderModule);
		if (expectedStage != module->stage)
		{
			EG_PANIC("Shader stage mismatch")
		}

		void* compilerMem = spvCompilers + pipeline->numShaderModules;
		spirv_cross::CompilerGLSL* compiler = new (compilerMem) spirv_cross::CompilerGLSL(module->parsedIR);

		SetSpecializationConstants(stageInfo, *compiler);

		GLuint shader = glCreateShader(ShaderTypes.at(static_cast<int>(expectedStage)));
		pipeline->shaderModules[pipeline->numShaderModules] = shader;
		shaderStages[pipeline->numShaderModules] = { compiler, shader };
		pipeline->numShaderModules++;

		if (useGLESPath)
		{
			// Renames interface variables
			for (uint32_t ivar : compiler->get_active_interface_variables())
			{
				spv::StorageClass storageClass = compiler->get_storage_class(ivar);
				uint32_t location = compiler->get_decoration(ivar, spv::DecorationLocation);
				if (storageClass == spv::StorageClassInput && expectedStage != ShaderStage::Vertex)
				{
					std::ostringstream nameStream;
					nameStream << "_io" << currentIOGroup << "_" << location;
					compiler->set_name(ivar, nameStream.str());
				}
				else if (storageClass == spv::StorageClassOutput && expectedStage != ShaderStage::Fragment)
				{
					std::ostringstream nameStream;
					nameStream << "_io" << (currentIOGroup + 1) << "_" << location;
					compiler->set_name(ivar, nameStream.str());
				}
			}
			currentIOGroup++;
		}

		if (createInfo.label != nullptr)
		{
			std::string shaderLabel = Concat({ createInfo.label, ShaderSuffixes.at(static_cast<int>(expectedStage)) });
			glObjectLabel(GL_SHADER, shader, -1, shaderLabel.c_str());
		}
	};
	MaybeAddStage(createInfo.vertexShader, eg::ShaderStage::Vertex);
	MaybeAddStage(createInfo.tessControlShader, eg::ShaderStage::TessControl);
	MaybeAddStage(createInfo.tessEvaluationShader, eg::ShaderStage::TessEvaluation);
	MaybeAddStage(createInfo.geometryShader, eg::ShaderStage::Geometry);
	MaybeAddStage(createInfo.fragmentShader, eg::ShaderStage::Fragment);

	pipeline->Initialize({ &shaderStages[0], pipeline->numShaderModules });

	for (uint32_t i = 0; i < pipeline->numShaderModules; i++)
	{
		spvCompilers[i].~CompilerGLSL();
	}
	spvCompilers = nullptr;

	if (createInfo.label != nullptr)
	{
		glObjectLabel(GL_PROGRAM, pipeline->program, -1, createInfo.label);
	}

	// ** Sets up VAOs **

	glGenVertexArrays(1, &pipeline->vertexArray);
	glBindVertexArray(pipeline->vertexArray);

	std::copy_n(createInfo.vertexBindings, MAX_VERTEX_BINDINGS, pipeline->vertexBindings);

	pipeline->numActiveVertexAttribs = 0;
	for (uint32_t i = 0; i < MAX_VERTEX_ATTRIBUTES; i++)
	{
		uint32_t binding = createInfo.vertexAttributes[i].binding;
		if (binding == UINT32_MAX)
			continue;

		GLVertexAttribFormat format = TranslateFormatForVertexAttribute(createInfo.vertexAttributes[i].format);

		pipeline->vertexAttribs[pipeline->numActiveVertexAttribs++] =
			VertexAttribData{ .attribIndex = i,
			                  .binding = createInfo.vertexAttributes[i].binding,
			                  .dataOffset = createInfo.vertexAttributes[i].offset,
			                  .format = format };

		glEnableVertexAttribArray(i);

#ifndef EG_GLES
		if (!useGLESPath)
		{
			glVertexAttribBinding(i, binding);

			if (format.mode == GLVertexAttribMode::Int)
			{
				glVertexAttribIFormat(i, format.size, format.type, createInfo.vertexAttributes[i].offset);
			}
			else
			{
				glVertexAttribFormat(
					i, format.size, format.type, format.mode == GLVertexAttribMode::Norm,
					createInfo.vertexAttributes[i].offset);
			}
		}
#endif
	}

	std::stable_sort(
		pipeline->vertexAttribs, pipeline->vertexAttribs + pipeline->numActiveVertexAttribs,
		[&](const VertexAttribData& a, const VertexAttribData& b) { return a.binding < b.binding; });

#ifndef EG_GLES
	if (!useGLESPath)
	{
		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
		{
			if (createInfo.vertexBindings[i].stride != UINT32_MAX)
			{
				glVertexBindingDivisor(i, static_cast<GLuint>(createInfo.vertexBindings[i].inputRate));
			}
		}
	}
#endif

#ifdef __EMSCRIPTEN__
	for (uint32_t i = 1; i < MAX_COLOR_ATTACHMENTS; i++)
	{
		if (createInfo.blendStates[i].enabled)
		{
			Log(LogLevel::Error, "gl", "Multi-target blend is not supported in WebGL");
			break;
		}
	}
#endif

	pipeline->enableScissorTest = createInfo.enableScissorTest;
	pipeline->enableDepthTest = createInfo.enableDepthTest;
	pipeline->enableDepthWrite = createInfo.enableDepthWrite;
	pipeline->enableStencilTest = createInfo.enableStencilTest;
	pipeline->topology = Translate(createInfo.topology);
	pipeline->wireframe = createInfo.wireframe;
	pipeline->patchSize = createInfo.patchControlPoints;

	if (createInfo.enableStencilTest)
	{
		TranslateStencilState(pipeline->backStencilState, createInfo.backStencilState);
		TranslateStencilState(pipeline->frontStencilState, createInfo.frontStencilState);
	}

	std::copy_n(createInfo.blendConstants, 4, pipeline->blendConstants);

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

void PipelineFramebufferFormatHint(PipelineHandle handle, const FramebufferFormatHint& hint) {}

void GraphicsPipeline::Free()
{
	for (uint32_t i = 0; i < numShaderModules; i++)
		glDeleteShader(shaderModules[i]);
	gfxPipelinePool.Delete(this);
}

static struct
{
	GLenum frontFace = GL_CCW;
	GLenum cullFace = GL_BACK;
	GLenum depthFunc = GL_LESS;
	GLint patchSize = 0;
	uint32_t numClipDistances = 0;
	uint32_t numCullDistances = 0;
	uint32_t stencilReferenceFront = 0;
	uint32_t stencilReferenceBack = 0;
	uint32_t stencilCompareMaskFront = 0;
	uint32_t stencilCompareMaskBack = 0;
	float minSampleShading = 0;
	bool wireframe = false;
	bool enableDepthWrite = true;
	bool blendEnabled[8] = {};
	float blendConstants[4] = {};
	ColorWriteMask colorWriteMasks[8] = {};
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
	if (!FEqual(currentViewport[0], x) || !FEqual(currentViewport[1], y) || !FEqual(currentViewport[2], w) ||
	    !FEqual(currentViewport[3], h))
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
	if (currentScissor[0] != x || currentScissor[1] != y || currentScissor[2] != w || currentScissor[3] != h)
	{
		currentScissor[0] = x;
		currentScissor[1] = y;
		currentScissor[2] = w;
		currentScissor[3] = h;
		scissorOutOfDate = true;
	}
}

void SetStencilValue(CommandContextHandle cc, StencilValue kind, uint32_t val)
{
	auto* graphicsPipeline = static_cast<const GraphicsPipeline*>(currentPipeline);

	int type = static_cast<int>(kind) & STENCIL_VALUE_MASK_VALUE;
	if (type == STENCIL_VALUE_WRITE_MASK)
	{
		GLenum face = 0;
		switch (static_cast<int>(kind) & 0b1100)
		{
		case 0b1000:
			face = GL_BACK;
			break;
		case 0b0100:
			face = GL_FRONT;
			break;
		case 0b1100:
			face = GL_FRONT_AND_BACK;
			break;
		default:
			EG_UNREACHABLE
		}

		glStencilMaskSeparate(face, val);
	}
	else
	{
		if (static_cast<int>(kind) & STENCIL_VALUE_MASK_BACK)
		{
			uint32_t newReference = curState.stencilReferenceBack;
			uint32_t newCompareMask = curState.stencilCompareMaskBack;

			if (type == STENCIL_VALUE_COMPARE_MASK)
				newCompareMask = val;
			else if (type == STENCIL_VALUE_REFERENCE)
				newReference = val;

			glStencilFuncSeparate(GL_BACK, graphicsPipeline->backStencilState.compareOp, newReference, newCompareMask);

			curState.stencilReferenceBack = newReference;
			curState.stencilCompareMaskBack = newCompareMask;
		}

		if (static_cast<int>(kind) & STENCIL_VALUE_MASK_FRONT)
		{
			uint32_t newReference = curState.stencilReferenceFront;
			uint32_t newCompareMask = curState.stencilCompareMaskFront;

			if (type == STENCIL_VALUE_COMPARE_MASK)
				newCompareMask = val;
			else if (type == STENCIL_VALUE_REFERENCE)
				newReference = val;

			glStencilFuncSeparate(
				GL_FRONT, graphicsPipeline->frontStencilState.compareOp, newReference, newCompareMask);

			curState.stencilReferenceFront = newReference;
			curState.stencilCompareMaskFront = newCompareMask;
		}
	}
}

inline bool IsScissorTestEnabled()
{
	if (currentPipeline == nullptr || !currentPipeline->isGraphicsPipeline)
		return false;
	return static_cast<const GraphicsPipeline*>(currentPipeline)->enableScissorTest;
}

void InitScissorTest()
{
	if (currentPipeline != nullptr && currentPipeline->isGraphicsPipeline)
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
		glViewport(
			static_cast<GLint>(std::round(currentViewport[0])), static_cast<GLint>(std::round(currentViewport[1])),
			static_cast<GLint>(std::round(currentViewport[2])), static_cast<GLint>(std::round(currentViewport[3])));
		viewportOutOfDate = false;
	}

	if (IsScissorTestEnabled() && scissorOutOfDate)
	{
		glScissor(currentScissor[0], currentScissor[1], currentScissor[2], currentScissor[3]);
		scissorOutOfDate = false;
	}
}

void GraphicsPipeline::Bind()
{
	AssertRenderPassActive("BindPipeline (Graphics)");

	glBindVertexArray(vertexArray);

	if (curState.frontFace != frontFace)
		glFrontFace(curState.frontFace = frontFace);
	if (curState.cullFace != cullFace)
		glCullFace(curState.cullFace = cullFace);
	if (enableDepthTest && curState.depthFunc != depthFunc)
		glDepthFunc(curState.depthFunc = depthFunc);

#ifndef __EMSCRIPTEN__
	if (curState.wireframe != wireframe)
	{
		glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
		curState.wireframe = wireframe;
	}
#endif

	SetEnabled<GL_CULL_FACE>(enableFaceCull);
	SetEnabled<GL_DEPTH_TEST>(enableDepthTest);
	SetEnabled<GL_STENCIL_TEST>(enableStencilTest);

	if (enableStencilTest)
	{
		if (backStencilState.failOp == frontStencilState.failOp &&
		    backStencilState.passOp == frontStencilState.passOp &&
		    backStencilState.depthFailOp == frontStencilState.depthFailOp)
		{
			glStencilOp(backStencilState.failOp, backStencilState.depthFailOp, backStencilState.passOp);
		}
		else
		{
			glStencilOpSeparate(
				GL_BACK, backStencilState.failOp, backStencilState.depthFailOp, backStencilState.passOp);
			glStencilOpSeparate(
				GL_FRONT, frontStencilState.failOp, frontStencilState.depthFailOp, frontStencilState.passOp);
		}

		if (backStencilState.writeMask == frontStencilState.writeMask)
		{
			glStencilMask(backStencilState.writeMask);
		}
		else
		{
			glStencilMaskSeparate(GL_BACK, backStencilState.writeMask);
			glStencilMaskSeparate(GL_FRONT, frontStencilState.writeMask);
		}

		glStencilFuncSeparate(
			GL_BACK, backStencilState.compareOp, backStencilState.reference, backStencilState.compareMask);
		glStencilFuncSeparate(
			GL_FRONT, frontStencilState.compareOp, frontStencilState.reference, frontStencilState.compareMask);

		curState.stencilCompareMaskBack = backStencilState.compareMask;
		curState.stencilCompareMaskFront = frontStencilState.compareMask;
		curState.stencilReferenceBack = backStencilState.reference;
		curState.stencilReferenceFront = frontStencilState.reference;
	}

	InitScissorTest();

#ifndef __EMSCRIPTEN__
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
#endif

	if (curState.enableDepthWrite != enableDepthWrite)
	{
		glDepthMask(static_cast<GLboolean>(enableDepthWrite));
		curState.enableDepthWrite = enableDepthWrite;
	}

	if (std::memcmp(curState.blendConstants, blendConstants, sizeof(float) * 4))
	{
		glBlendColor(blendConstants[0], blendConstants[1], blendConstants[2], blendConstants[3]);
		std::copy_n(blendConstants, 4, curState.blendConstants);
	}

#ifdef __EMSCRIPTEN__
	if (curState.colorWriteMasks[0] != colorWriteMasks[0])
	{
		glColorMask(
			HasFlag(colorWriteMasks[0], ColorWriteMask::R), HasFlag(colorWriteMasks[0], ColorWriteMask::G),
			HasFlag(colorWriteMasks[0], ColorWriteMask::B), HasFlag(colorWriteMasks[0], ColorWriteMask::A));
		curState.colorWriteMasks[0] = colorWriteMasks[0];
	}
	SetEnabled<GL_BLEND>(blend[0].enabled);
	if (blend[0].enabled)
	{
		glBlendEquationSeparate(blend[0].colorFunc, blend[0].alphaFunc);
		glBlendFuncSeparate(
			blend[0].srcColorFactor, blend[0].dstColorFactor, blend[0].srcAlphaFactor, blend[0].dstAlphaFactor);
	}
#else
	for (GLuint i = 0; i < 8; i++)
	{
		if (curState.colorWriteMasks[i] != colorWriteMasks[i])
		{
			glColorMaski(
				i, HasFlag(colorWriteMasks[i], ColorWriteMask::R), HasFlag(colorWriteMasks[i], ColorWriteMask::G),
				HasFlag(colorWriteMasks[i], ColorWriteMask::B), HasFlag(colorWriteMasks[i], ColorWriteMask::A));
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
			glBlendFuncSeparatei(
				i, blend[i].srcColorFactor, blend[i].dstColorFactor, blend[i].srcAlphaFactor, blend[i].dstAlphaFactor);
		}
	}
#endif

	updateVAOBindings = true;
}

static IndexType currentIndexType;
static uint32_t indexBufferOffset;
static GLuint indexBuffer;
static uint32_t currentFirstVertex = 0;
static uint32_t currentFirstInstance = 0;
static std::pair<GLuint, uint32_t> vertexBuffers[MAX_VERTEX_BINDINGS];

inline void MaybeUpdateVAO(uint32_t firstVertex, uint32_t firstInstance)
{
	if (useGLESPath && (firstVertex != currentFirstVertex || firstInstance != currentFirstInstance))
		updateVAOBindings = true;

	if (!updateVAOBindings)
		return;
	updateVAOBindings = false;
	currentFirstVertex = firstVertex;
	currentFirstInstance = firstInstance;

	const GraphicsPipeline* pipeline = static_cast<const GraphicsPipeline*>(currentPipeline);

	if (useGLESPath)
	{
		uint32_t binding = UINT32_MAX;
		std::for_each_n(
			pipeline->vertexAttribs, pipeline->numActiveVertexAttribs,
			[&](const VertexAttribData& attrib)
			{
				if (binding != attrib.binding)
				{
					binding = attrib.binding;
					glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[binding].first);
				}

				const GLsizei stride = pipeline->vertexBindings[binding].stride;

				const uint32_t first =
					pipeline->vertexBindings[binding].inputRate == InputRate::Vertex ? firstVertex : firstInstance;

				uintptr_t offset = attrib.dataOffset + vertexBuffers[binding].second + first * stride;
				void* offsetPtr = reinterpret_cast<void*>(offset);

				if (attrib.format.mode == GLVertexAttribMode::Int)
				{
					glVertexAttribIPointer(
						attrib.attribIndex, attrib.format.size, attrib.format.type, stride, offsetPtr);
				}
				else
				{
					glVertexAttribPointer(
						attrib.attribIndex, attrib.format.size, attrib.format.type,
						attrib.format.mode == GLVertexAttribMode::Norm, stride, offsetPtr);
				}

				glVertexAttribDivisor(
					attrib.attribIndex, static_cast<GLuint>(pipeline->vertexBindings[binding].inputRate));
			});
	}
	else
	{
#ifndef EG_GLES
		for (uint32_t binding = 0; binding < MAX_VERTEX_BINDINGS; binding++)
		{
			if (pipeline->vertexBindings[binding].stride != UINT32_MAX)
			{
				glBindVertexBuffer(
					binding, vertexBuffers[binding].first, vertexBuffers[binding].second,
					pipeline->vertexBindings[binding].stride);
			}
		}
#endif
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
}

void BindVertexBuffer(CommandContextHandle, uint32_t binding, BufferHandle buffer, uint32_t offset)
{
	AssertRenderPassActive("BindVertexBuffer");
	vertexBuffers[binding] = std::make_pair(reinterpret_cast<Buffer*>(buffer)->buffer, offset);
	updateVAOBindings = true;
}

void BindIndexBuffer(CommandContextHandle, IndexType type, BufferHandle buffer, uint32_t offset)
{
	AssertRenderPassActive("BindIndexBuffer");
	currentIndexType = type;
	indexBuffer = reinterpret_cast<Buffer*>(buffer)->buffer;
	indexBufferOffset = offset;
	updateVAOBindings = true;
}

void Draw(
	CommandContextHandle, uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances)
{
	AssertRenderPassActive("Draw");
	AssertAllBindingsSatisfied();

	CommitViewportAndScissor();
	MaybeUpdateVAO(0, firstInstance);

	GLenum topology = static_cast<const GraphicsPipeline*>(currentPipeline)->topology;

	if (useGLESPath)
	{
		glDrawArraysInstanced(topology, firstVertex, numVertices, numInstances);
	}
	else
	{
#ifndef EG_GLES
		glDrawArraysInstancedBaseInstance(topology, firstVertex, numVertices, numInstances, firstInstance);
#endif
	}

	ClearBarriers();
}

void DrawIndexed(
	CommandContextHandle, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance,
	uint32_t numInstances)
{
	AssertRenderPassActive("DrawIndexed");
	AssertAllBindingsSatisfied();

	CommitViewportAndScissor();
	MaybeUpdateVAO(firstVertex, firstInstance);

	uintptr_t indexOffset = indexBufferOffset + firstIndex * 2;
	GLenum indexType = GL_UNSIGNED_SHORT;

	if (currentIndexType == IndexType::UInt32)
	{
		indexType = GL_UNSIGNED_INT;
		indexOffset += firstIndex * 2;
	}
	void* indexOffsetPtr = reinterpret_cast<void*>(indexOffset);

	GLenum topology = static_cast<const GraphicsPipeline*>(currentPipeline)->topology;

	if (useGLESPath)
	{
		glDrawElementsInstanced(topology, numIndices, indexType, indexOffsetPtr, numInstances);
	}
	else
	{
#ifndef EG_GLES
		glDrawElementsInstancedBaseVertexBaseInstance(
			topology, numIndices, indexType, indexOffsetPtr, numInstances, firstVertex, firstInstance);
#endif
	}

	ClearBarriers();
}
} // namespace eg::graphics_api::gl
