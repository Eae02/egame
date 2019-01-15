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
		m_pipeline = program.CreatePipeline(ffs);
	}
	
	void RunFrame(float dt) override
	{
		eg::DC.SetViewport(0, 0, eg::CurrentResolutionX(), eg::CurrentResolutionY());
		eg::DC.ClearColor(0, eg::Color(0.2f, 1.0f, 1.0f));
		eg::DC.BindPipeline(m_pipeline);
		eg::DC.Draw(0, 3, 1);
	}
	
private:
	eg::Pipeline m_pipeline;
	eg::EventListener<eg::ResolutionChangedEvent> m_listener;
};

int main()
{
	return eg::Run<Game>();
}
