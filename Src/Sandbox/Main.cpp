#include "EGame/EG.hpp"
#include <EGame/Graphics/PBR/BRDFIntegrationMap.hpp>

class Game : public eg::IGame
{
public:
	Game()
	{
		eg::LoadAssets(eg::ExeRelPath("SandboxAssets"), "/");
		
		eg::GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModule>("Main.vs.glsl").Handle();
		pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Main.fs.glsl").Handle();
		m_pipeline = eg::Pipeline::Create(pipelineCI);
		
		m_pipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	}
	
	void RunFrame(float dt) override
	{
		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
		rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(eg::ColorSRGB(0.2f, 1.0f, 1.0f));
		eg::DC.BeginRenderPass(rpBeginInfo);
		
		eg::DC.BindPipeline(m_pipeline);
		
		const float SIZE = std::min(eg::CurrentResolutionX(), eg::CurrentResolutionY()) * 0.8f;
		glm::vec2 scale(SIZE / eg::CurrentResolutionX(), SIZE / eg::CurrentResolutionY());
		
		glm::mat2 transform = glm::scale(glm::mat3(1.0f), scale) * glm::rotate(glm::mat3(1.0f), m_rotation);
		eg::DC.PushConstants(0, transform);
		
		eg::DC.Draw(0, 3, 0, 1);
		
		eg::DC.EndRenderPass();
		
		m_rotation += dt;
	}
	
private:
	float m_rotation = 0.0f;
	eg::Pipeline m_pipeline;
	eg::BRDFIntegrationMap m_integMap;
};

int main(int argc, char** argv)
{
	eg::RunConfig runConfig;
	runConfig.gameName = "EGame Sandbox";
	runConfig.flags = eg::RunFlags::DevMode | eg::RunFlags::DefaultFramebufferSRGB;
	
	for (int i = 1; i < argc; i++)
	{
		std::string_view arg = argv[i];
		if (arg == "--gl")
			runConfig.graphicsAPI = eg::GraphicsAPI::OpenGL;
		else if (arg == "--vk")
			runConfig.graphicsAPI = eg::GraphicsAPI::Vulkan;
	}
	
	return eg::Run<Game>(runConfig);
}
