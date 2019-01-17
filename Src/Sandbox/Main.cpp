#include "EGame/EG.hpp"

class Game : public eg::IGame
{
public:
	Game()
	{
		eg::LoadAssets(eg::ExeRelPath("SandboxAssets"), "/");
		
		eg::ShaderProgram program;
		program.AddStageFromAsset("Main.vs.glsl");
		program.AddStageFromAsset("Main.fs.glsl");
		eg::FixedFuncState ffs;
		ffs.depthFormat = eg::Format::DefaultDepthStencil;
		ffs.attachments[0].format = eg::Format::DefaultColor;
		m_pipeline = program.CreatePipeline(ffs);
	}
	
	void RunFrame(float dt) override
	{
		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
		rpBeginInfo.colorAttachments[0].clearValue = eg::Color(0.2f, 1.0f, 1.0f);
		eg::DC.BeginRenderPass(rpBeginInfo);
		
		eg::DC.SetViewport(0, 0, eg::CurrentResolutionX(), eg::CurrentResolutionY());
		eg::DC.BindPipeline(m_pipeline);
		eg::DC.Draw(0, 3, 1);
		
		eg::DC.EndRenderPass();
	}
	
private:
	eg::Pipeline m_pipeline;
	eg::EventListener<eg::ResolutionChangedEvent> m_listener;
};

int main()
{
	eg::RunConfig runConfig;
	runConfig.gameName = "EGame Sandbox";
	runConfig.flags = eg::RunFlags::DevMode;
	runConfig.graphicsAPI = eg::GraphicsAPI::Vulkan;
	
	return eg::Run<Game>(runConfig);
}
