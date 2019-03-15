#include "OpenGL.hpp"
#include "Utils.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"
#include "../../Span.hpp"
#include "../../Log.hpp"
#include "OpenGLBuffer.hpp"

#include <atomic>
#include <GL/gl3w.h>
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

namespace eg::graphics_api::gl
{
	using SPIRType = spirv_cross::SPIRType;
	
	struct PushConstantMember
	{
		uint32_t offset;
		uint32_t arraySize;
		uint32_t vectorSize;
		uint32_t columns;
		int uniformLocation;
		SPIRType::BaseType baseType;
	};
	
	struct ShaderModule
	{
		GLuint handle;
		ShaderStage stage;
		std::atomic_int ref;
		spirv_cross::CompilerGLSL spvCompiler;
		
		ShaderModule(Span<const char> code)
			: ref(1), spvCompiler(reinterpret_cast<const uint32_t*>(code.data()), code.size() / 4) { }
		
		void Unref();
	};
	
	static ObjectPool<ShaderModule> shaderModulePool;
	
	void ShaderModule::Unref()
	{
		if (ref-- == 1)
		{
			MainThreadInvoke([this]
			{
				glDeleteShader(handle);
				shaderModulePool.Delete(this);
			});
		}
	}
	
	inline ShaderModule* UnwrapShaderModule(ShaderModuleHandle handle)
	{
		return reinterpret_cast<ShaderModule*>(handle);
	}
	
	inline std::optional<UniformType> GetUniformType(GLenum glType)
	{
		switch (glType)
		{
		case GL_INT: return UniformType::Int;
		case GL_FLOAT: return UniformType::Float;
		case GL_FLOAT_VEC2: return UniformType::Vec2;
		case GL_FLOAT_VEC3: return UniformType::Vec3;
		case GL_FLOAT_VEC4: return UniformType::Vec4;
		case GL_INT_VEC2: return UniformType::IVec2;
		case GL_INT_VEC3: return UniformType::IVec3;
		case GL_INT_VEC4: return UniformType::IVec4;
		case GL_FLOAT_MAT3: return UniformType::Mat3;
		case GL_FLOAT_MAT4: return UniformType::Mat4;
		default: return { };
		}
	}
	
	//Indices must match ShaderStage
	static const GLenum ShaderTypes[] =
	{
		GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER
	};
	
	ShaderModuleHandle CreateShaderModule(ShaderStage stage, Span<const char> code)
	{
		ShaderModule* module = shaderModulePool.New(code);
		module->stage = stage;
		
		for (spirv_cross::SpecializationConstant& specConst : module->spvCompiler.get_specialization_constants())
		{
			spirv_cross::SPIRConstant& spirConst = module->spvCompiler.get_constant(specConst.id);
			if (specConst.constant_id == 500)
			{
				spirConst.m.c[0].r[0].u32 = 1;
			}
		}
		
		std::string glslCode = module->spvCompiler.compile();
		
		module->handle = glCreateShader(ShaderTypes[(int)stage]);
		
		const GLchar* glslCodeC = glslCode.c_str();
		GLint glslCodeLen = glslCode.size();
		glShaderSource(module->handle, 1, &glslCodeC, &glslCodeLen);
		
		glCompileShader(module->handle);
		
		//Checks the shader's compile status.
		GLint compileStatus = GL_FALSE;
		glGetShaderiv(module->handle, GL_COMPILE_STATUS, &compileStatus);
		if (!compileStatus)
		{
			GLint infoLogLen = 0;
			glGetShaderiv(module->handle, GL_INFO_LOG_LENGTH, &infoLogLen);
			
			std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
			glGetShaderInfoLog(module->handle, infoLogLen, nullptr, infoLog.data());
			infoLog.back() = '\0';
			
			EG_PANIC("Shader failed to compile:" << infoLog.data());
		}
		
		return reinterpret_cast<ShaderModuleHandle>(module);
	}
	
	void DestroyShaderModule(ShaderModuleHandle handle)
	{
		UnwrapShaderModule(handle)->Unref();
	}
	
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
	
	struct Pipeline
	{
		GLuint program;
		GLuint vertexArray;
		bool enableFaceCull;
		GLenum frontFace;
		GLenum cullFace;
		GLenum depthFunc;
		GLenum topology;
		GLint patchSize;
		bool enableScissorTest;
		bool enableDepthTest;
		bool enableDepthWrite;
		BlendState blend[8];
		uint32_t maxVertexBinding;
		VertexBinding vertexBindings[MAX_VERTEX_BINDINGS];
		std::vector<PushConstantMember> pushConstants;
		ShaderModule* shaderModules[5];
	};
	
	inline Pipeline* UnwrapPipeline(PipelineHandle handle)
	{
		return reinterpret_cast<Pipeline*>(handle);
	}
	
	static ObjectPool<Pipeline> pipelinePool;
	
	inline GLenum Translate(BlendFunc f)
	{
		switch (f)
		{
		case BlendFunc::Add: return GL_FUNC_ADD;
		case BlendFunc::Subtract: return GL_FUNC_SUBTRACT;
		case BlendFunc::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
		case BlendFunc::Min: return GL_MIN;
		case BlendFunc::Max: return GL_MAX;
		}
		EG_UNREACHABLE
	}
	
	inline GLenum Translate(BlendFactor f)
	{
		switch (f)
		{
			case BlendFactor::Zero: return GL_ZERO;
			case BlendFactor::One: return GL_ONE;
			case BlendFactor::SrcColor: return GL_SRC_COLOR;
			case BlendFactor::OneMinusSrcColor: return GL_SRC_COLOR;
			case BlendFactor::DstColor: return GL_DST_COLOR;
			case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
			case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
			case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
			case BlendFactor::DstAlpha: return GL_DST_ALPHA;
			case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
		}
		EG_UNREACHABLE
	}
	
	inline GLenum Translate(Topology t)
	{
		switch (t)
		{
		case Topology::TriangleList: return GL_TRIANGLES;
		case Topology::TriangleStrip: return GL_TRIANGLE_STRIP;
		case Topology::TriangleFan: return GL_TRIANGLE_FAN;
		case Topology::LineList: return GL_LINES;
		case Topology::LineStrip: return GL_LINE_STRIP;
		case Topology::Points: return GL_POINTS;
		case Topology::Patches: return GL_PATCHES;
		}
		EG_UNREACHABLE
	}
	
	PipelineHandle CreatePipeline(const PipelineCreateInfo& createInfo)
	{
		Pipeline* pipeline = pipelinePool.New();
		std::fill_n(pipeline->shaderModules, 5, nullptr);
		
		pipeline->program = glCreateProgram();
		
		size_t numStages = 0;
		spirv_cross::CompilerGLSL* spvCompilers[5];
		
		//Attaches shaders to the pipeline's program
		auto MaybeAddStage = [&] (ShaderModuleHandle handle, ShaderStage expectedStage)
		{
			if (handle != nullptr)
			{
				ShaderModule* module = UnwrapShaderModule(handle);
				if (expectedStage != module->stage)
				{
					EG_PANIC("Shader stage mismatch");
				}
				module->ref++;
				glAttachShader(pipeline->program, module->handle);
				spvCompilers[numStages] = &module->spvCompiler;
				pipeline->shaderModules[numStages] = module;
				numStages++;
			}
		};
		MaybeAddStage(createInfo.vertexShader, eg::ShaderStage::Vertex);
		MaybeAddStage(createInfo.fragmentShader, eg::ShaderStage::Fragment);
		MaybeAddStage(createInfo.geometryShader, eg::ShaderStage::Geometry);
		MaybeAddStage(createInfo.tessControlShader, eg::ShaderStage::TessControl);
		MaybeAddStage(createInfo.tessEvaluationShader, eg::ShaderStage::TessEvaluation);
		
		glLinkProgram(pipeline->program);
		
		//Checks that the program linked successfully
		int linkStatus = GL_FALSE;
		glGetProgramiv(pipeline->program, GL_LINK_STATUS, &linkStatus);
		if (!linkStatus)
		{
			int infoLogLen = 0;
			glGetProgramiv(pipeline->program, GL_INFO_LOG_LENGTH, &infoLogLen);
			
			std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
			glGetProgramInfoLog(pipeline->program, infoLogLen, nullptr, infoLog.data());
			infoLog.back() = '\0';
			
			EG_PANIC("Shader program failed to link: " << infoLog.data());
		}
		
		std::for_each(spvCompilers, spvCompilers + numStages, [&] (const spirv_cross::CompilerGLSL* spvCrossCompiler)
		{
			const spirv_cross::ShaderResources& resources = spvCrossCompiler->get_shader_resources();
			
			for (const spirv_cross::Resource& pcBlock : resources.push_constant_buffers)
			{
				const SPIRType& type = spvCrossCompiler->get_type(pcBlock.base_type_id);
				uint32_t numMembers = type.member_types.size();
				
				std::string blockName = spvCrossCompiler->get_name(pcBlock.id);
				if (blockName.empty())
				{
					blockName = spvCrossCompiler->get_fallback_name(pcBlock.id);
				}
				
				for (uint32_t i = 0; i < numMembers; i++)
				{
					const SPIRType& memberType = spvCrossCompiler->get_type(type.member_types[i]);
					
					//Only process supported base types
					static const SPIRType::BaseType supportedBaseTypes[] = 
					{
						SPIRType::Float, SPIRType::Int, SPIRType::UInt, SPIRType::Boolean
					};
					if (!Contains(supportedBaseTypes, memberType.basetype))
						continue;
					
					//Gets the name and uniform location of this member
					const std::string& name = spvCrossCompiler->get_member_name(type.self, i);
					std::string uniformName = Concat({ blockName, ".", name });
					int location = glGetUniformLocation(pipeline->program, uniformName.c_str());
					if (location == -1)
					{
						Log(LogLevel::Error, "gl", "Internal OpenGL error, push constant uniform not found: '{0}' "
							"(expected name: '{1}')", name, uniformName);
						continue;
					}
					
					if (memberType.columns != 1 && memberType.columns != memberType.vecsize)
					{
						Log(LogLevel::Error, "gl", "Push constant '{0}': non square matrices are not currently "
							"supported as push constants.", name);
						continue;
					}
					
					PushConstantMember& pushConstant = pipeline->pushConstants.emplace_back();
					pushConstant.uniformLocation = location;
					pushConstant.arraySize = 1;
					pushConstant.offset = spvCrossCompiler->type_struct_member_offset(type, i);
					pushConstant.baseType = memberType.basetype;
					pushConstant.vectorSize = memberType.vecsize;
					pushConstant.columns = memberType.columns;
					
					for (uint32_t arraySize : memberType.array)
						pushConstant.arraySize *= arraySize;
				}
			}
		});
		
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
		
		switch (createInfo.depthCompare)
		{
		case CompareOp::Never: pipeline->depthFunc = GL_NEVER; break;
		case CompareOp::Less: pipeline->depthFunc = GL_LESS; break;
		case CompareOp::Equal: pipeline->depthFunc = GL_EQUAL; break;
		case CompareOp::LessOrEqual: pipeline->depthFunc = GL_LEQUAL; break;
		case CompareOp::Greater: pipeline->depthFunc = GL_GREATER; break;
		case CompareOp::NotEqual: pipeline->depthFunc = GL_NOTEQUAL; break;
		case CompareOp::GreaterOrEqual: pipeline->depthFunc = GL_GEQUAL; break;
		case CompareOp::Always: pipeline->depthFunc = GL_ALWAYS; break;
		}
		
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
	
	void DestroyPipeline(PipelineHandle handle)
	{
		Pipeline* pipeline = UnwrapPipeline(handle);
		
		for (ShaderModule* module : pipeline->shaderModules)
		{
			if (module != nullptr)
				module->Unref();
		}
		
		MainThreadInvoke([pipeline]
		{
			glDeleteProgram(pipeline->program);
			pipelinePool.Free(pipeline);
		});
	}
	
	static struct
	{
		GLenum frontFace = GL_CCW;
		GLenum cullFace = GL_BACK;
		GLenum depthFunc = GL_LESS;
		GLint patchSize = 0;
		bool enableDepthWrite = true;
		bool blendEnabled[8] = { };
	} curState;
	
	static bool updateVAOBindings = false;
	static const Pipeline* currentPipeline;
	
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
	
	void InitScissorTest()
	{
		if (currentPipeline != nullptr)
			SetEnabled<GL_SCISSOR_TEST>(currentPipeline->enableScissorTest);
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
		
		if (currentPipeline->enableScissorTest && scissorOutOfDate)
		{
			glScissorArrayv(0, 1, currentScissor);
			scissorOutOfDate = false;
		}
	}
	
	void BindPipeline(CommandContextHandle, PipelineHandle handle)
	{
		const Pipeline* pipeline = UnwrapPipeline(handle);
		if (pipeline == currentPipeline)
			return;
		currentPipeline = pipeline;
		
		glUseProgram(pipeline->program);
		glBindVertexArray(pipeline->vertexArray);
		
		if (curState.frontFace != pipeline->frontFace)
			glFrontFace(curState.frontFace = pipeline->frontFace);
		if (curState.cullFace != pipeline->cullFace)
			glCullFace(curState.cullFace = pipeline->cullFace);
		if (pipeline->enableDepthTest && curState.depthFunc != pipeline->depthFunc)
			glDepthFunc(curState.depthFunc = pipeline->depthFunc);
		
		SetEnabled<GL_CULL_FACE>(pipeline->enableFaceCull);
		SetEnabled<GL_DEPTH_TEST>(pipeline->enableDepthTest);
		
		InitScissorTest();
		
		if (pipeline->patchSize != 0 && curState.patchSize != pipeline->patchSize)
		{
			glPatchParameteri(GL_PATCH_VERTICES, pipeline->patchSize);
			curState.patchSize = pipeline->patchSize;
		}
		
		if (curState.enableDepthWrite != pipeline->enableDepthWrite)
		{
			glDepthMask(static_cast<GLboolean>(pipeline->enableDepthWrite));
			curState.enableDepthWrite = pipeline->enableDepthWrite;
		}
		
		for (GLuint i = 0; i < 8; i++)
		{
			if (curState.blendEnabled[i] != pipeline->blend[i].enabled)
			{
				if (pipeline->blend[i].enabled)
					glEnablei(GL_BLEND, i);
				else
					glDisablei(GL_BLEND, i);
				curState.blendEnabled[i] = pipeline->blend[i].enabled;
			}
			if (pipeline->blend[i].enabled)
			{
				glBlendEquationSeparatei(i, pipeline->blend[i].colorFunc, pipeline->blend[i].alphaFunc);
				glBlendFuncSeparatei(i, pipeline->blend[i].srcColorFactor, pipeline->blend[i].dstColorFactor,
					pipeline->blend[i].srcAlphaFactor, pipeline->blend[i].dstAlphaFactor);
			}
		}
		
		updateVAOBindings = true;
	}
	
	template <typename T>
	struct SetUniformFunctions
	{
		void(*Set1)(GLint location, GLsizei count, const T* value);
		void(*Set2)(GLint location, GLsizei count, const T* value);
		void(*Set3)(GLint location, GLsizei count, const T* value);
		void(*Set4)(GLint location, GLsizei count, const T* value);
		void(*SetMatrix2)(GLint location, GLsizei count, GLboolean transpose, const T* value);
		void(*SetMatrix3)(GLint location, GLsizei count, GLboolean transpose, const T* value);
		void(*SetMatrix4)(GLint location, GLsizei count, GLboolean transpose, const T* value);
	};
	
	template <typename T>
	inline void SetPushConstantUniform(const SetUniformFunctions<T>& func, const PushConstantMember& pushConst,
		uint32_t offset, uint32_t range, const void* data)
	{
		const T* value = reinterpret_cast<const T*>(reinterpret_cast<const char*>(data) + (pushConst.offset - offset));
		if (pushConst.columns == 1)
		{
			if (pushConst.vectorSize == 1)
			{
				func.Set1(pushConst.uniformLocation, pushConst.arraySize, value);
			}
			else if (pushConst.vectorSize == 2)
			{
				func.Set2(pushConst.uniformLocation, pushConst.arraySize, value);
			}
			else if (pushConst.vectorSize == 3)
			{
				T* packedValues = reinterpret_cast<T*>(alloca(pushConst.arraySize * sizeof(T) * 3));
				for (uint32_t i = 0; i < pushConst.arraySize; i++)
				{
					std::memcpy(packedValues + (i * 3), value + (i * 4), sizeof(T) * 3);
				}
				func.Set3(pushConst.uniformLocation, pushConst.arraySize, packedValues);
			}
			else if (pushConst.vectorSize == 4)
			{
				func.Set4(pushConst.uniformLocation, pushConst.arraySize, value);
			}
		}
		else if (pushConst.columns == 2)
		{
			func.SetMatrix2(pushConst.uniformLocation, pushConst.arraySize, GL_FALSE, value);
		}
		else if (pushConst.columns == 3)
		{
			T* packedValues = reinterpret_cast<T*>(alloca(pushConst.arraySize * sizeof(T) * 9));
			for (uint32_t i = 0; i < pushConst.arraySize * 3; i++)
			{
				std::memcpy(packedValues + (i * 3), value + (i * 4), sizeof(T) * 3);
			}
			func.SetMatrix3(pushConst.uniformLocation, pushConst.arraySize, GL_FALSE, packedValues);
		}
		else if (pushConst.columns == 4)
		{
			func.SetMatrix4(pushConst.uniformLocation, pushConst.arraySize, GL_FALSE, value);
		}
	}
	
	void PushConstants(CommandContextHandle, uint32_t offset, uint32_t range, const void* data)
	{
		for (const PushConstantMember& pushConst : currentPipeline->pushConstants)
		{
			if (pushConst.offset < offset && pushConst.offset >= offset + range)
				continue;
			
			switch (pushConst.baseType)
			{
			case SPIRType::Float:
			{
				SetUniformFunctions<float> func =
				{
					glUniform1fv, glUniform2fv, glUniform3fv, glUniform4fv,
					glUniformMatrix2fv, glUniformMatrix3fv, glUniformMatrix4fv
				};
				SetPushConstantUniform<float>(func, pushConst, offset, range, data);
				break;
			}
			case SPIRType::Boolean:
			case SPIRType::Int:
			{
				SetUniformFunctions<int32_t> func = { glUniform1iv, glUniform2iv, glUniform3iv, glUniform4iv };
				SetPushConstantUniform<int32_t>(func, pushConst, offset, range, data);
				break;
			}
			case SPIRType::UInt:
			{
				SetUniformFunctions<uint32_t> func = { glUniform1uiv, glUniform2uiv, glUniform3uiv, glUniform4uiv };
				SetPushConstantUniform<uint32_t>(func, pushConst, offset, range, data);
				break;
			}
			default:
				EG_PANIC("Unknown push constant type.");
			}
		}
	}
	
	static IndexType currentIndexType;
	static uint32_t indexBufferOffset;
	static GLuint indexBuffer;
	static std::pair<GLuint, uint32_t> vertexBuffers[MAX_VERTEX_BINDINGS];
	
	inline void MaybeUpdateVAO()
	{
		if (!updateVAOBindings)
			return;
		
		for (uint32_t i = 0; i < currentPipeline->maxVertexBinding; i++)
		{
			if (currentPipeline->vertexBindings[i].stride != UINT32_MAX)
			{
				glBindVertexBuffer(i, vertexBuffers[i].first, vertexBuffers[i].second,
				                   currentPipeline->vertexBindings[i].stride);
			}
		}
		
		glVertexArrayElementBuffer(currentPipeline->vertexArray, indexBuffer);
		
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
		glDrawArraysInstancedBaseInstance(currentPipeline->topology, firstVertex, numVertices, numInstances, firstInstance);
	}
	
	void DrawIndexed(CommandContextHandle, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance, uint32_t numInstances)
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
		
		glDrawElementsInstancedBaseVertexBaseInstance(currentPipeline->topology, numIndices, indexType,
			(void*)indexOffset, numInstances, firstVertex, firstInstance);
	}
}
