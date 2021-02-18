#include "EGame/EG.hpp"
#include <EGame/Graphics/PBR/BRDFIntegrationMap.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>

class Game : public eg::IGame
{
public:
	Game()
	{
		if (!eg::LoadAssets(eg::ExeRelPath("SandboxAssets"), "/"))
            std::exit(1);
		
		eg::GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Main.vs.glsl").DefaultVariant();
		pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Main.fs.glsl").DefaultVariant();
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
};

#ifdef __EMSCRIPTEN__
extern "C" void WebMain()
{
	eg::RunConfig runConfig;
	runConfig.gameName = "EGame Sandbox";
	runConfig.flags = eg::RunFlags::DevMode | eg::RunFlags::DefaultFramebufferSRGB;
	
	eg::Run<Game>(runConfig);
}
#else
int main(int argc, char** argv)
{
	eg::RunConfig runConfig;
	runConfig.gameName = "EGame Sandbox";
	runConfig.flags = eg::RunFlags::DevMode | eg::RunFlags::DefaultFramebufferSRGB;
	
	eg::ParseCommandLineArgs(runConfig, argc, argv);
	
	return eg::Run<Game>(runConfig);
}
#endif
