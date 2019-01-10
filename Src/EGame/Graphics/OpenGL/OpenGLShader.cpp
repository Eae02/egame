#include "OpenGL.hpp"
#include "Utils.hpp"
#include "../Graphics.hpp"
#include "../../Alloc/ObjectPool.hpp"
#include "../../MainThreadInvoke.hpp"
#include "../../Span.hpp"
#include "../../Log.hpp"
#include "OpenGLBuffer.hpp"

#include <set>
#include <GL/gl3w.h>

namespace eg::graphics_api::gl
{
	struct Uniform
	{
		uint32_t nameHash;
		UniformType type;
		uint32_t arraySize;
		uint32_t location;
	};
	
	struct ShaderProgram
	{
		int ref;
		GLuint program;
		uint32_t numShaders;
		GLuint shaders[2];
		std::vector<Uniform> uniforms;
		std::set<uint32_t> warnedUniformNames;
	};
	
	static ObjectPool<ShaderProgram> shaderProgramPool;
	
	bool supportsSpirV;
	
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
		
		for (const ShaderStageDesc& stage : stages)
		{
			GLuint shader = glCreateShader(GetGLStage(stage.stage));
			
			if (supportsSpirV)
			{
				glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, stage.code, stage.codeBytes);
				glSpecializeShader(shader, "main", 0, nullptr, nullptr);
			}
			else
			{
				//TODO:
			}
			
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
		
		int numUniforms;
		glGetProgramiv(program->program, GL_ACTIVE_UNIFORMS, &numUniforms);
		program->uniforms.reserve((size_t)numUniforms);
		
		int uniformMaxLen;
		glGetProgramiv(program->program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniformMaxLen);
		char* nameBuffer = reinterpret_cast<char*>(alloca(uniformMaxLen));
		
		//Reads uniforms
		GLint uniformSize;
		GLenum uniformType;
		for (GLuint i = 0; i < (GLuint)numUniforms; i++)
		{
			glGetActiveUniform(program->program, i, uniformMaxLen, nullptr, &uniformSize, &uniformType, nameBuffer);
			
			if (std::optional<UniformType> type = GetUniformType(uniformType))
			{
				program->uniforms.push_back({ HashFNV1a32(nameBuffer), *type, (uint32_t)uniformSize, i });
			}
		}
		
		//Sorts uniforms so that they can be binary searched over later
		std::sort(program->uniforms.begin(), program->uniforms.end(), [&] (const Uniform& a, const Uniform& b)
		{
			return a.nameHash < b.nameHash;
		});
		
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
		bool enableStencilTest;
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
				DataType::UInt8Norm, DataType::UInt16Norm, DataType::UInt32Norm,
				DataType::SInt8Norm, DataType::SInt16Norm, DataType::SInt32Norm
			};
			
			DataType type = fixedFuncState.vertexAttributes[i].type;
			GLenum glType = TranslateDataType(type);
			
			if (eg::Contains(intDataTypes, type))
			{
				glVertexArrayAttribIFormat(pipeline->vertexArray, i, fixedFuncState.vertexAttributes[i].size,
					glType, fixedFuncState.vertexAttributes[i].offset);
			}
			else
			{
				auto normalized = static_cast<GLboolean>(eg::Contains(normDataTypes, type));
				glVertexArrayAttribFormat(pipeline->vertexArray, i, fixedFuncState.vertexAttributes[i].size,
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
		
		pipeline->enableStencilTest = fixedFuncState.enableStencilTest;
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
		SetEnabled<GL_STENCIL_TEST>(pipeline->enableStencilTest);
		SetEnabled<GL_DEPTH_TEST>(pipeline->enableDepthTest);
		
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
	
	void SetUniform(CommandContextHandle, ShaderProgramHandle programHandle, std::string_view name, UniformType type,
		uint32_t count, const void* value)
	{
		ShaderProgram* program = UnwrapShaderProgram(programHandle);
		if (program == nullptr)
			program = currentPipeline->program;
		
		const uint32_t nameHash = HashFNV1a32(name);
		auto it = std::lower_bound(program->uniforms.begin(), program->uniforms.end(), nameHash,
		                           [&] (const Uniform& a, uint32_t b) { return a.nameHash < b; });
		
		auto WarnOnce = [&]
		{
			if (program->warnedUniformNames.find(nameHash) == program->warnedUniformNames.end())
				return false;
			program->warnedUniformNames.insert(nameHash);
			return true;
		};
		
		if (it == program->uniforms.end() || it->nameHash != nameHash)
		{
			if (WarnOnce())
			{
				Log(LogLevel::Error, "gfx", "Uniform not found '{0}'", name);
			}
			return;
		}
		
		//Checks that the type is correct
		if (type != it->type)
		{
			if (WarnOnce())
			{
				Log(LogLevel::Error, "gfx", "Uniform type mismatch for '{0}', expected {1} but got {2}.",
					name, it->type, type);
			}
			return;
		}
		
		//Clamps the count if it is out of range
		if (count > it->arraySize)
		{
			if (WarnOnce())
			{
				Log(LogLevel::Warning, "gfx", "Uniform size mismatch for '{0}', maximum is {1} but got {2}, "
					"only the first {1} values will be set.", name, it->arraySize, count);
			}
			count = it->arraySize;
		}
		
		switch (it->type)
		{
		case UniformType::Float:
			glProgramUniform1fv(program->program, it->location, count, reinterpret_cast<const float*>(value));
			break;
		case UniformType::Vec2:
			glProgramUniform2fv(program->program, it->location, count, reinterpret_cast<const float*>(value));
			break;
		case UniformType::Vec3:
			glProgramUniform3fv(program->program, it->location, count, reinterpret_cast<const float*>(value));
			break;
		case UniformType::Vec4:
			glProgramUniform4fv(program->program, it->location, count, reinterpret_cast<const float*>(value));
			break;
		case UniformType::Int:
			glProgramUniform1iv(program->program, it->location, count, reinterpret_cast<const int*>(value));
			break;
		case UniformType::IVec2:
			glProgramUniform2iv(program->program, it->location, count, reinterpret_cast<const int*>(value));
			break;
		case UniformType::IVec3:
			glProgramUniform3iv(program->program, it->location, count, reinterpret_cast<const int*>(value));
			break;
		case UniformType::IVec4:
			glProgramUniform4iv(program->program, it->location, count, reinterpret_cast<const int*>(value));
			break;
		case UniformType::Mat3:
			glProgramUniformMatrix3fv(program->program, it->location, count, GL_FALSE, reinterpret_cast<const float*>(value));
			break;
		case UniformType::Mat4:
			glProgramUniformMatrix4fv(program->program, it->location, count, GL_FALSE, reinterpret_cast<const float*>(value));
			break;
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
	
	void Draw(CommandContextHandle, uint32_t firstVertex, uint32_t numVertices, uint32_t numInstances)
	{
		MaybeUpdateVAO();
		glDrawArraysInstanced(curState.topology, firstVertex, numVertices, numInstances);
	}
	
	void DrawIndexed(CommandContextHandle, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t numInstances)
	{
		MaybeUpdateVAO();
		
		uintptr_t indexOffset = indexBufferOffset + firstIndex * 2;
		GLenum indexType = GL_UNSIGNED_SHORT;
		
		if (currentIndexType == IndexType::UInt32)
		{
			indexType = GL_UNSIGNED_INT;
			indexOffset += firstIndex * 2;
		}
		
		glDrawElementsInstancedBaseVertex(curState.topology, numIndices, indexType,
			(void*)indexOffset, numInstances, firstVertex);
	}
}
