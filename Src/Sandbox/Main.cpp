#include "EGame/EG.hpp"
#include "Main.vert.h"
#include "Main.frag.h"

class Game : public eg::IGame
{
public:
	Game()
	{
		eg::ShaderProgram program;
		program.AddStageBorrowedCode(eg::ShaderStage::Vertex, eg::Span<const char>((const char*)SpvVS, sizeof(SpvVS)));
		program.AddStageBorrowedCode(eg::ShaderStage::Fragment, eg::Span<const char>((const char*)SpvFS, sizeof(SpvFS)));
		eg::FixedFuncState ffs;
		m_pipeline = program.CreatePipeline(ffs);
	}
	
	void Update() override
	{
		
	}
	
	void Draw() override
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
