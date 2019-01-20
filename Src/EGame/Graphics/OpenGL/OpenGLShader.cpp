#include "OpenGL.hpp"
#include "Utils.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"
#include "../../Span.hpp"
#include "../../Log.hpp"
#include "OpenGLBuffer.hpp"

#include <list>
#include <GL/gl3w.h>
#include <spirv_cross.hpp>
#include <spirv_glsl.hpp>

namespace eg::graphics_api::gl
{
	using SPIRType = spirv_cross::SPIRType;
	
	static uint32_t PUSH_CONSTANT_MEMBER_SIZES[] =
	{
		/* Int   */ 4,
		/* Float */ 4,
		/* Vec2  */ 8,
		/* Vec3  */ 16,
		/* Vec4  */ 16,
		/* IVec2 */ 8,
		/* IVec3 */ 16,
		/* IVec4 */ 16,
		/* Mat2  */ 16,
		/* Mat3  */ 64,
		/* Mat4  */ 64
	};
	
	struct PushConstantMember
	{
		uint32_t offset;
		uint32_t arraySize;
		uint32_t vectorSize;
		uint32_t columns;
		int uniformLocation;
		SPIRType::BaseType baseType;
	};
	
	struct ShaderProgram
	{
		int ref;
		GLuint program;
		uint32_t numShaders;
		GLuint shaders[2];
		std::vector<PushConstantMember> pushConstants;
	};
	
	static ObjectPool<ShaderProgram> shaderProgramPool;
	
	inline GLenum GetGLStage(ShaderStage stage)
	{
		switch (stage)
		{
		case ShaderStage::Vertex: return GL_VERTEX_SHADER;
		case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
		}
		EG_UNREACHABLE
	}
	
	inline ShaderProgram* UnwrapShaderProgram(ShaderProgramHandle handle)
	{
		return reinterpret_cast<ShaderProgram*>(handle);
	}
	
	inline void UnrefProgram(ShaderProgram* program)
	{
		program->ref--;
		if (program->ref == 0)
		{
			for (uint32_t i = 0; i < program->numShaders; i++)
				glDeleteShader(program->shaders[i]);
			glDeleteProgram(program->program);
			shaderProgramPool.Delete(program);
		}
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
	
	ShaderProgramHandle CreateShaderProgram(Span<const ShaderStageDesc> stages)
	{
		ShaderProgram* program = shaderProgramPool.New();
		program->ref = 1;
		program->numShaders = 0;
		program->program = glCreateProgram();
		
		std::list<spirv_cross::CompilerGLSL> spvCompilers;
		
		for (const ShaderStageDesc& stage : stages)
		{
			spirv_cross::CompilerGLSL& spvCrossCompiler = spvCompilers.emplace_back(
				reinterpret_cast<const uint32_t*>(stage.code), stage.codeBytes / 4);
			
			std::string glslCode = spvCrossCompiler.compile();
			
			GLuint shader = glCreateShader(GetGLStage(stage.stage));
			
			const GLchar* glslCodeC = glslCode.c_str();
			GLint glslCodeLen = glslCode.size();
			glShaderSource(shader, 1, &glslCodeC, &glslCodeLen);
			
			glCompileShader(shader);
			
			//Checks the shader's compile status.
			GLint compileStatus = GL_FALSE;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
			if (!compileStatus)
			{
				GLint infoLogLen = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
				
				std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
				glGetShaderInfoLog(shader, infoLogLen, nullptr, infoLog.data());
				infoLog.back() = '\0';
				
				EG_PANIC("Shader failed to compile:" << infoLog.data());
			}
			
			glAttachShader(program->program, shader);
			program->shaders[program->numShaders++] = shader;
		}
		
		glLinkProgram(program->program);
		
		//Checks that the program linked successfully
		int linkStatus = GL_FALSE;
		glGetProgramiv(program->program, GL_LINK_STATUS, &linkStatus);
		if (!linkStatus)
		{
			int infoLogLen = 0;
			glGetProgramiv(program->program, GL_INFO_LOG_LENGTH, &infoLogLen);
			
			std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
			glGetProgramInfoLog(program->program, infoLogLen, nullptr, infoLog.data());
			infoLog.back() = '\0';
			
			EG_PANIC("Shader program failed to link: " << infoLog.data());
		}
		
		for (const spirv_cross::CompilerGLSL& spvCrossCompiler : spvCompilers)
		{
			const spirv_cross::ShaderResources& resources = spvCrossCompiler.get_shader_resources();
			
			for (const spirv_cross::Resource& pcBlock : resources.push_constant_buffers)
			{
				const SPIRType& type = spvCrossCompiler.get_type(pcBlock.base_type_id);
				uint32_t numMembers = type.member_types.size();
				
				std::string_view blockName = spvCrossCompiler.get_name(pcBlock.id);
				if (blockName.empty())
				{
					blockName = spvCrossCompiler.get_fallback_name(pcBlock.id);
				}
				
				for (uint32_t i = 0; i < numMembers; i++)
				{
					const SPIRType& memberType = spvCrossCompiler.get_type(type.member_types[i]);
					
					//Only process supported base types
					static const SPIRType::BaseType supportedBaseTypes[] = 
					{
						SPIRType::Float, SPIRType::Int, SPIRType::UInt, SPIRType::Boolean
					};
					if (!Contains(supportedBaseTypes, memberType.basetype))
						continue;
					
					//Gets the name and uniform location of this member
					const std::string& name = spvCrossCompiler.get_member_name(type.self, i);
					std::string uniformName = Concat({ blockName, ".", name });
					int location = glGetUniformLocation(program->program, uniformName.c_str());
					if (location == -1)
					{
						Log(LogLevel::Error, "gl", "Internal OpenGL error, push constant uniform not found: '{0}'", name);
						continue;
					}
					
					if (memberType.columns != 1 && memberType.columns != memberType.vecsize)
					{
						Log(LogLevel::Error, "gl", "Push constant '{0}': non square matrices are not currently "
							"supported as push constants.", name);
						continue;
					}
					
					PushConstantMember& pushConstant = program->pushConstants.emplace_back();
					pushConstant.uniformLocation = location;
					pushConstant.arraySize = 1;
					pushConstant.offset = spvCrossCompiler.type_struct_member_offset(type, i);
					pushConstant.baseType = memberType.basetype;
					pushConstant.vectorSize = memberType.vecsize;
					pushConstant.columns = memberType.columns;
					
					Log(LogLevel::Info, "gl", "Found push constant '{0}' at offset {1}", name, pushConstant.offset);
					
					for (uint32_t arraySize : memberType.array)
						pushConstant.arraySize *= arraySize;
				}
			}
		}
		
		return reinterpret_cast<ShaderProgramHandle>(program);
	}
	
	void DestroyShaderProgram(ShaderProgramHandle handle)
	{
		MainThreadInvoke([handle]
		{
			UnrefProgram(UnwrapShaderProgram(handle));
		});
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
		ShaderProgram* program;
		GLuint vertexArray;
		bool enableFaceCull;
		GLenum frontFace;
		GLenum cullFace;
		GLenum depthFunc;
		bool enableScissorTest;
		bool enableDepthTest;
		bool enableDepthWrite;
		BlendState blend[8];
		uint32_t maxVertexBinding;
		VertexBinding vertexBindings[MAX_VERTEX_BINDINGS];
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
	
	PipelineHandle CreatePipeline(ShaderProgramHandle program, const FixedFuncState& fixedFuncState)
	{
		Pipeline* pipeline = pipelinePool.New();
		pipeline->program = UnwrapShaderProgram(program);
		pipeline->program->ref++;
		
		glCreateVertexArrays(1, &pipeline->vertexArray);
		for (uint32_t i = 0; i < MAX_VERTEX_ATTRIBUTES; i++)
		{
			uint32_t binding = fixedFuncState.vertexAttributes[i].binding;
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
			
			DataType type = fixedFuncState.vertexAttributes[i].type;
			GLenum glType = TranslateDataType(type);
			
			if (eg::Contains(intDataTypes, type))
			{
				glVertexArrayAttribIFormat(pipeline->vertexArray, i, fixedFuncState.vertexAttributes[i].components,
					glType, fixedFuncState.vertexAttributes[i].offset);
			}
			else
			{
				auto normalized = static_cast<GLboolean>(eg::Contains(normDataTypes, type));
				glVertexArrayAttribFormat(pipeline->vertexArray, i, fixedFuncState.vertexAttributes[i].components,
					glType, normalized, fixedFuncState.vertexAttributes[i].offset);
			}
		}
		
		pipeline->maxVertexBinding = 0;
		for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; i++)
		{
			pipeline->vertexBindings[i] = fixedFuncState.vertexBindings[i];
			if (fixedFuncState.vertexBindings[i].stride == UINT32_MAX)
				continue;
			glVertexArrayBindingDivisor(pipeline->vertexArray, i, (GLuint)fixedFuncState.vertexBindings[i].inputRate);
			pipeline->maxVertexBinding = i + 1;
		}
		
		pipeline->enableScissorTest = fixedFuncState.enableScissorTest;
		pipeline->enableDepthTest = fixedFuncState.enableDepthTest;
		pipeline->enableDepthWrite = fixedFuncState.enableDepthWrite;
		
		switch (fixedFuncState.cullMode)
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
		
		switch (fixedFuncState.depthCompare)
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
			bool enabled = pipeline->blend[i].enabled = fixedFuncState.attachments[i].blend.enabled;
			if (enabled)
			{
				pipeline->blend[i].colorFunc = Translate(fixedFuncState.attachments[i].blend.colorFunc);
				pipeline->blend[i].alphaFunc = Translate(fixedFuncState.attachments[i].blend.alphaFunc);
				pipeline->blend[i].srcColorFactor = Translate(fixedFuncState.attachments[i].blend.srcColorFactor);
				pipeline->blend[i].srcAlphaFactor = Translate(fixedFuncState.attachments[i].blend.srcAlphaFactor);
				pipeline->blend[i].dstColorFactor = Translate(fixedFuncState.attachments[i].blend.dstColorFactor);
				pipeline->blend[i].dstAlphaFactor = Translate(fixedFuncState.attachments[i].blend.dstAlphaFactor);
			}
		}
		
		pipeline->frontFace = fixedFuncState.frontFaceCCW ? GL_CCW : GL_CW;
		
		return reinterpret_cast<PipelineHandle>(pipeline);
	}
	
	void DestroyPipeline(PipelineHandle handle)
	{
		Pipeline* pipeline = UnwrapPipeline(handle);
		MainThreadInvoke([pipeline]
		{
			UnrefProgram(pipeline->program);
			pipelinePool.Free(pipeline);
		});
	}
	
	static struct
	{
		GLenum frontFace = GL_CCW;
		GLenum cullFace = GL_BACK;
		GLenum topology = GL_TRIANGLES;
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
		
		glUseProgram(pipeline->program->program);
		glBindVertexArray(pipeline->vertexArray);
		
		if (curState.frontFace != pipeline->frontFace)
			glFrontFace(curState.frontFace = pipeline->frontFace);
		if (curState.cullFace != pipeline->cullFace)
			glCullFace(curState.cullFace = pipeline->cullFace);
		
		SetEnabled<GL_CULL_FACE>(pipeline->enableFaceCull);
		SetEnabled<GL_DEPTH_TEST>(pipeline->enableDepthTest);
		
		InitScissorTest();
		
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
		for (const PushConstantMember& pushConst : currentPipeline->program->pushConstants)
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
		glDrawArraysInstancedBaseInstance(curState.topology, firstVertex, numVertices, numInstances, firstInstance);
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
		
		glDrawElementsInstancedBaseVertexBaseInstance(curState.topology, numIndices, indexType,
			(void*)indexOffset, numInstances, firstVertex, firstInstance);
	}
}
