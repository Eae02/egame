#include "EGame/EG.hpp"
#include <glm/gtx/matrix_transform_2d.hpp>
#include <string_view>

#ifdef EG_HAS_IMGUI
#include <EGameImGui.hpp>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

bool useIMGUI = true;

struct Game : public eg::IGame
{
	Game()
	{
		if (!eg::LoadAssets("SandboxAssets", "/", eg::GetDefaultEnabledAssetSideStreams()))
		{
			EG_PANIC("Error loading assets");
		}

		m_pipeline = eg::Pipeline::Create(eg::GraphicsPipelineCreateInfo{
			.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Main.vs.glsl").ToStageInfo(),
			.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Main.fs.glsl").ToStageInfo(),
			.numColorAttachments = 1,
			.colorAttachmentFormats = { eg::Format::DefaultColor },
			.depthAttachmentFormat = eg::Format::DefaultDepthStencil,
		});

		m_parametersBuffer =
			eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::UniformBuffer, sizeof(glm::mat2), nullptr);

		m_descriptorSet = eg::DescriptorSet(m_pipeline, 0);
		m_descriptorSet.BindUniformBuffer(m_parametersBuffer, 0);

#ifdef EG_HAS_IMGUI
		if (useIMGUI)
		{
			eg::imgui::Initialize(eg::imgui::InitializeArgs());
		}
#endif
	}

	void RunFrame(float dt) override
	{
		const float SIZE = std::min(eg::CurrentResolutionX(), eg::CurrentResolutionY()) * 0.8f;
		glm::vec2 scale(SIZE / eg::CurrentResolutionX(), SIZE / eg::CurrentResolutionY());
		glm::mat2 transform = glm::scale(glm::mat3(1.0f), scale) * glm::rotate(glm::mat3(1.0f), m_rotation);

		m_parametersBuffer.DCUpdateData(0, sizeof(glm::mat2), &transform);
		m_parametersBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex);

		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Clear;
		rpBeginInfo.colorAttachments[0].clearValue = eg::ColorLin(eg::ColorSRGB(0.2f, 1.0f, 1.0f));
		eg::DC.BeginRenderPass(rpBeginInfo);

		eg::DC.BindPipeline(m_pipeline);
		eg::DC.BindDescriptorSet(m_descriptorSet, 0);

		eg::DC.Draw(0, 3, 0, 1);

		eg::DC.EndRenderPass();

#ifdef EG_HAS_IMGUI
		if (useIMGUI)
		{
			ImGui::SliderFloat("Rotation Speed", &m_rotationSpeed, -2.0f, 2.0f);
		}
#endif

		m_rotation += dt * m_rotationSpeed;
	}

	float m_rotationSpeed = 1.0f;
	float m_rotation = 0.0f;
	eg::Pipeline m_pipeline;
	eg::Buffer m_parametersBuffer;
	eg::DescriptorSet m_descriptorSet;
};

#ifdef __EMSCRIPTEN__
extern "C" void EMSCRIPTEN_KEEPALIVE WebMain()
{
	eg::DownloadAssetPackageASync(eg::DownloadAssetPackageArgs{ .eapName = "SandboxAssets.eap", .cacheID = "cid" });
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

	for (int i = 1; i < argc; i++)
	{
		if (std::string_view(argv[i]) == "--no-imgui")
			useIMGUI = false;
	}

	eg::ParseCommandLineArgs(runConfig, argc, argv);

	return eg::Run<Game>(runConfig);
}
#endif
