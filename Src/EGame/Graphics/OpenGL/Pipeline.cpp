#include "Pipeline.hpp"
#include "../../MainThreadInvoke.hpp"

namespace eg::graphics_api::gl
{
	const AbstractPipeline* currentPipeline;
	
	uint32_t ResolveBinding(const AbstractPipeline& pipeline, uint32_t set, uint32_t binding)
	{
		auto it = std::lower_bound(pipeline.bindings.begin(), pipeline.bindings.end(), MappedBinding { set, binding });
		return it->glBinding;
	}
	
	void DestroyPipeline(PipelineHandle handle)
	{
		MainThreadInvoke([pipeline=UnwrapPipeline(handle)]
		{
			glDeleteProgram(pipeline->program);
			pipeline->Free();
		});
	}
	
	void BindPipeline(CommandContextHandle, PipelineHandle handle)
	{
		AbstractPipeline* pipeline = UnwrapPipeline(handle);
		if (pipeline == currentPipeline)
			return;
		currentPipeline = pipeline;
		
		glUseProgram(pipeline->program);
		pipeline->Bind();
	}
	
	void AbstractPipeline::Initialize(uint32_t numShaderModules, spirv_cross::CompilerGLSL** spvCompilers,
		GLuint* shaderModules)
	{
		//Detects resources used in shaders
		std::for_each(spvCompilers, spvCompilers + numShaderModules,
			[&] (const spirv_cross::CompilerGLSL* spvCompiler)
		{
			auto ProcessResources = [&] (const std::vector<spirv_cross::Resource>& resources, BindingType type)
			{
				for (const spirv_cross::Resource& res : resources)
				{
					const uint32_t set = spvCompiler->get_decoration(res.id, spv::DecorationDescriptorSet);
					const uint32_t binding = spvCompiler->get_decoration(res.id, spv::DecorationBinding);
					bool exists = std::any_of(bindings.begin(), bindings.end(),
						[&] (const MappedBinding& mb) { return mb.set == set && mb.binding == binding; });
					if (!exists)
					{
						bindings.push_back({ set, binding, type, 0 });
					}
				}
			};
			
			const spirv_cross::ShaderResources& resources = spvCompiler->get_shader_resources();
			ProcessResources(resources.uniform_buffers, BindingType::UniformBuffer);
			ProcessResources(resources.sampled_images, BindingType::Texture);
			ProcessResources(resources.storage_images, BindingType::StorageImage);
		});
		
		std::sort(bindings.begin(), bindings.end());
		
		//Assigns gl bindings to resources
		uint32_t nextTextureBinding = 0;
		uint32_t nextStorageImageBinding = 0;
		uint32_t nextUniformBufferBinding = 0;
		for (uint32_t i = 0; i < bindings.size(); i++)
		{
			uint32_t set = bindings[i].set;
			if (i == 0 || bindings[i - 1].set != set)
			{
				sets[set] = { 0, 0, 0, 0, nextUniformBufferBinding, nextTextureBinding, nextStorageImageBinding };
			}
			sets[set].maxBinding = std::max(sets[set].maxBinding, bindings[i].binding);
			switch (bindings[i].type)
			{
			case BindingType::UniformBuffer:
				sets[set].numUniformBuffers++;
				bindings[i].glBinding = nextUniformBufferBinding++;
				break;
			case BindingType::Texture:
				sets[set].numTextures++;
				bindings[i].glBinding = nextTextureBinding++;
				break;
			case BindingType::StorageImage:
				sets[set].numStorageImages++;
				bindings[i].glBinding = nextStorageImageBinding++;
				break;
			}
		}
		
		program = glCreateProgram();
		
		//Updates the bindings used by resources and uploads code to shader modules
		for (uint32_t i = 0; i < numShaderModules; i++)
		{
			auto ProcessResources = [&] (const std::vector<spirv_cross::Resource>& resources)
			{
				for (const spirv_cross::Resource& res : resources)
				{
					const uint32_t set = spvCompilers[i]->get_decoration(res.id, spv::DecorationDescriptorSet);
					const uint32_t binding = spvCompilers[i]->get_decoration(res.id, spv::DecorationBinding);
					auto it = std::lower_bound(bindings.begin(), bindings.end(), MappedBinding { set, binding });
					spvCompilers[i]->set_decoration(res.id, spv::DecorationDescriptorSet, 0);
					spvCompilers[i]->set_decoration(res.id, spv::DecorationBinding, it->glBinding);
				}
			};
			
			const spirv_cross::ShaderResources& resources = spvCompilers[i]->get_shader_resources();
			ProcessResources(resources.uniform_buffers);
			ProcessResources(resources.sampled_images);
			ProcessResources(resources.storage_images);
			
			spirv_cross::CompilerGLSL::Options options = spvCompilers[i]->get_common_options();
			options.version = 430;
			spvCompilers[i]->set_common_options(options);
			std::string glslCode = spvCompilers[i]->compile();
			
			const GLchar* glslCodeC = glslCode.c_str();
			const GLint glslCodeLen = (GLint)glslCode.size();
			glShaderSource(shaderModules[i], 1, &glslCodeC, &glslCodeLen);
			
			glCompileShader(shaderModules[i]);
			
			//Checks the shader's compile status.
			GLint compileStatus = GL_FALSE;
			glGetShaderiv(shaderModules[i], GL_COMPILE_STATUS, &compileStatus);
			if (!compileStatus)
			{
				GLint infoLogLen = 0;
				glGetShaderiv(shaderModules[i], GL_INFO_LOG_LENGTH, &infoLogLen);
				
				std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
				glGetShaderInfoLog(shaderModules[i], infoLogLen, nullptr, infoLog.data());
				infoLog.back() = '\0';
				
				std::cout << "Shader failed to compile!\n\n --- GLSL Code --- \n" << glslCode <<
					"\n\n --- Info Log --- \n" << infoLog.data() << std::endl;
				
				std::abort();
			}
			
			glAttachShader(program, shaderModules[i]);
		}
		
		glLinkProgram(program);
		
		//Checks that the program linked successfully
		int linkStatus = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (!linkStatus)
		{
			int infoLogLen = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
			
			std::vector<char> infoLog(static_cast<size_t>(infoLogLen) + 1);
			glGetProgramInfoLog(program, infoLogLen, nullptr, infoLog.data());
			infoLog.back() = '\0';
			
			EG_PANIC("Shader program failed to link: " << infoLog.data());
		}
		
		//Processes push constant blocks
		std::for_each(spvCompilers, spvCompilers + numShaderModules,
			[&] (const spirv_cross::CompilerGLSL* spvCrossCompiler)
		{
			const spirv_cross::ShaderResources& resources = spvCrossCompiler->get_shader_resources();
			
			for (const spirv_cross::Resource& pcBlock : resources.push_constant_buffers)
			{
				const SPIRType& type = spvCrossCompiler->get_type(pcBlock.base_type_id);
				uint32_t numMembers = (uint32_t)type.member_types.size();
				
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
					int location = glGetUniformLocation(program, uniformName.c_str());
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
					
					PushConstantMember& pushConstant = pushConstants.emplace_back();
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
}
