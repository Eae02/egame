#include "EGameImGui.hpp"

#include "../EGame/Core.hpp"
#include "../EGame/Event.hpp"
#include "../EGame/InputState.hpp"
#include "../EGame/Platform/FileSystem.hpp"

#include "../Shaders/Build/ImGui.fs.h"
#include "../Shaders/Build/ImGui.vs.h"
#include "EGame/Platform/FontConfig.hpp"

namespace eg::imgui
{
static bool isInitialized;

static eg::EventListener<eg::ButtonEvent>* buttonEventListener;

static Texture fontTexture;

static Sampler textureSampler;

static ShaderModule vertexShader;
static ShaderModule fragmentShader;
static Pipeline pipeline;

static int vertexBufferCapacity;
static Buffer vertexBuffer;

static int indexBufferCapacity;
static Buffer indexBuffer;

static Buffer scaleUniformBuffer;
static DescriptorSet scaleUniformBufferDescriptorSet;

static ImGuiKey buttonRemapTable[NUM_BUTTONS];

void StartFrame(float dt);
void EndFrame();

void Initialize(const InitializeArgs& args)
{
	if (isInitialized)
		return;
	isInitialized = true;

	ImGui::CreateContext();

	// ** Initializes ImGui IO **
	ImGuiIO& io = ImGui::GetIO();

#pragma region buttonRemapTable
	buttonRemapTable[static_cast<int>(Button::Tab)] = ImGuiKey_Tab;
	buttonRemapTable[static_cast<int>(Button::LeftArrow)] = ImGuiKey_LeftArrow;
	buttonRemapTable[static_cast<int>(Button::RightArrow)] = ImGuiKey_RightArrow;
	buttonRemapTable[static_cast<int>(Button::UpArrow)] = ImGuiKey_UpArrow;
	buttonRemapTable[static_cast<int>(Button::DownArrow)] = ImGuiKey_DownArrow;
	buttonRemapTable[static_cast<int>(Button::PageUp)] = ImGuiKey_PageUp;
	buttonRemapTable[static_cast<int>(Button::PageDown)] = ImGuiKey_PageDown;
	buttonRemapTable[static_cast<int>(Button::Home)] = ImGuiKey_Home;
	buttonRemapTable[static_cast<int>(Button::End)] = ImGuiKey_End;
	buttonRemapTable[static_cast<int>(Button::Delete)] = ImGuiKey_Delete;
	buttonRemapTable[static_cast<int>(Button::Backspace)] = ImGuiKey_Backspace;
	buttonRemapTable[static_cast<int>(Button::Space)] = ImGuiKey_Space;
	buttonRemapTable[static_cast<int>(Button::Enter)] = ImGuiKey_Enter;
	buttonRemapTable[static_cast<int>(Button::Escape)] = ImGuiKey_Escape;
	// buttonRemapTable[static_cast<int>(Button::Apostrophe)] = ImGuiKey_Apostrophe;
	// buttonRemapTable[static_cast<int>(Button::Comma)] = ImGuiKey_Comma;
	// buttonRemapTable[static_cast<int>(Button::Minus)] = ImGuiKey_Minus;
	// buttonRemapTable[static_cast<int>(Button::Period)] = ImGuiKey_Period;
	// buttonRemapTable[static_cast<int>(Button::Slash)] = ImGuiKey_Slash;
	// buttonRemapTable[static_cast<int>(Button::Semicolon)] = ImGuiKey_Semicolon;
	// buttonRemapTable[static_cast<int>(Button::Equal)] = ImGuiKey_Equal;
	// buttonRemapTable[static_cast<int>(Button::LeftBracket)] = ImGuiKey_LeftBracket;
	// buttonRemapTable[static_cast<int>(Button::Backslash)] = ImGuiKey_Backslash;
	// buttonRemapTable[static_cast<int>(Button::RightBracket)] = ImGuiKey_RightBracket;
	buttonRemapTable[static_cast<int>(Button::Grave)] = ImGuiKey_GraveAccent;
	// buttonRemapTable[static_cast<int>(Button::CapsLock)] = ImGuiKey_CapsLock;
	// buttonRemapTable[static_cast<int>(Button::ScrollLock)] = ImGuiKey_ScrollLock;
	// buttonRemapTable[static_cast<int>(Button::NumLock)] = ImGuiKey_NumLock;
	// buttonRemapTable[static_cast<int>(Button::PrintScreen)] = ImGuiKey_PrintScreen;
	// buttonRemapTable[static_cast<int>(Button::Pause)] = ImGuiKey_Pause;
	// buttonRemapTable[static_cast<int>(Button::Keypad0)] = ImGuiKey_Keypad0;
	// buttonRemapTable[static_cast<int>(Button::Keypad1)] = ImGuiKey_Keypad1;
	// buttonRemapTable[static_cast<int>(Button::Keypad2)] = ImGuiKey_Keypad2;
	// buttonRemapTable[static_cast<int>(Button::Keypad3)] = ImGuiKey_Keypad3;
	// buttonRemapTable[static_cast<int>(Button::Keypad4)] = ImGuiKey_Keypad4;
	// buttonRemapTable[static_cast<int>(Button::Keypad5)] = ImGuiKey_Keypad5;
	// buttonRemapTable[static_cast<int>(Button::Keypad6)] = ImGuiKey_Keypad6;
	// buttonRemapTable[static_cast<int>(Button::Keypad7)] = ImGuiKey_Keypad7;
	// buttonRemapTable[static_cast<int>(Button::Keypad8)] = ImGuiKey_Keypad8;
	// buttonRemapTable[static_cast<int>(Button::Keypad9)] = ImGuiKey_Keypad9;
	// buttonRemapTable[static_cast<int>(Button::KeypadDecimal)] = ImGuiKey_KeypadDecimal;
	// buttonRemapTable[static_cast<int>(Button::KeypadDivide)] = ImGuiKey_KeypadDivide;
	// buttonRemapTable[static_cast<int>(Button::KeypadMultiply)] = ImGuiKey_KeypadMultiply;
	// buttonRemapTable[static_cast<int>(Button::KeypadSubtract)] = ImGuiKey_KeypadSubtract;
	// buttonRemapTable[static_cast<int>(Button::KeypadAdd)] = ImGuiKey_KeypadAdd;
	// buttonRemapTable[static_cast<int>(Button::KeypadEnter)] = ImGuiKey_KeypadEnter;
	// buttonRemapTable[static_cast<int>(Button::KeypadEqual)] = ImGuiKey_KeypadEqual;
	buttonRemapTable[static_cast<int>(Button::LeftShift)] = ImGuiKey_LeftShift;
	buttonRemapTable[static_cast<int>(Button::LeftControl)] = ImGuiKey_LeftCtrl;
	buttonRemapTable[static_cast<int>(Button::LeftAlt)] = ImGuiKey_LeftAlt;
	buttonRemapTable[static_cast<int>(Button::RightShift)] = ImGuiKey_RightShift;
	buttonRemapTable[static_cast<int>(Button::RightControl)] = ImGuiKey_RightCtrl;
	buttonRemapTable[static_cast<int>(Button::RightAlt)] = ImGuiKey_RightAlt;
	buttonRemapTable[static_cast<int>(Button::D0)] = ImGuiKey_0;
	buttonRemapTable[static_cast<int>(Button::D1)] = ImGuiKey_1;
	buttonRemapTable[static_cast<int>(Button::D2)] = ImGuiKey_2;
	buttonRemapTable[static_cast<int>(Button::D3)] = ImGuiKey_3;
	buttonRemapTable[static_cast<int>(Button::D4)] = ImGuiKey_4;
	buttonRemapTable[static_cast<int>(Button::D5)] = ImGuiKey_5;
	buttonRemapTable[static_cast<int>(Button::D6)] = ImGuiKey_6;
	buttonRemapTable[static_cast<int>(Button::D7)] = ImGuiKey_7;
	buttonRemapTable[static_cast<int>(Button::D8)] = ImGuiKey_8;
	buttonRemapTable[static_cast<int>(Button::D9)] = ImGuiKey_9;
	buttonRemapTable[static_cast<int>(Button::A)] = ImGuiKey_A;
	buttonRemapTable[static_cast<int>(Button::B)] = ImGuiKey_B;
	buttonRemapTable[static_cast<int>(Button::C)] = ImGuiKey_C;
	buttonRemapTable[static_cast<int>(Button::D)] = ImGuiKey_D;
	buttonRemapTable[static_cast<int>(Button::E)] = ImGuiKey_E;
	buttonRemapTable[static_cast<int>(Button::F)] = ImGuiKey_F;
	buttonRemapTable[static_cast<int>(Button::G)] = ImGuiKey_G;
	buttonRemapTable[static_cast<int>(Button::H)] = ImGuiKey_H;
	buttonRemapTable[static_cast<int>(Button::I)] = ImGuiKey_I;
	buttonRemapTable[static_cast<int>(Button::J)] = ImGuiKey_J;
	buttonRemapTable[static_cast<int>(Button::K)] = ImGuiKey_K;
	buttonRemapTable[static_cast<int>(Button::L)] = ImGuiKey_L;
	buttonRemapTable[static_cast<int>(Button::M)] = ImGuiKey_M;
	buttonRemapTable[static_cast<int>(Button::N)] = ImGuiKey_N;
	buttonRemapTable[static_cast<int>(Button::O)] = ImGuiKey_O;
	buttonRemapTable[static_cast<int>(Button::P)] = ImGuiKey_P;
	buttonRemapTable[static_cast<int>(Button::Q)] = ImGuiKey_Q;
	buttonRemapTable[static_cast<int>(Button::R)] = ImGuiKey_R;
	buttonRemapTable[static_cast<int>(Button::S)] = ImGuiKey_S;
	buttonRemapTable[static_cast<int>(Button::T)] = ImGuiKey_T;
	buttonRemapTable[static_cast<int>(Button::U)] = ImGuiKey_U;
	buttonRemapTable[static_cast<int>(Button::V)] = ImGuiKey_V;
	buttonRemapTable[static_cast<int>(Button::W)] = ImGuiKey_W;
	buttonRemapTable[static_cast<int>(Button::X)] = ImGuiKey_X;
	buttonRemapTable[static_cast<int>(Button::Y)] = ImGuiKey_Y;
	buttonRemapTable[static_cast<int>(Button::Z)] = ImGuiKey_Z;
	buttonRemapTable[static_cast<int>(Button::F1)] = ImGuiKey_F1;
	buttonRemapTable[static_cast<int>(Button::F2)] = ImGuiKey_F2;
	buttonRemapTable[static_cast<int>(Button::F3)] = ImGuiKey_F3;
	buttonRemapTable[static_cast<int>(Button::F4)] = ImGuiKey_F4;
	buttonRemapTable[static_cast<int>(Button::F5)] = ImGuiKey_F5;
	buttonRemapTable[static_cast<int>(Button::F6)] = ImGuiKey_F6;
	buttonRemapTable[static_cast<int>(Button::F7)] = ImGuiKey_F7;
	buttonRemapTable[static_cast<int>(Button::F8)] = ImGuiKey_F8;
	buttonRemapTable[static_cast<int>(Button::F9)] = ImGuiKey_F9;
	buttonRemapTable[static_cast<int>(Button::F10)] = ImGuiKey_F10;
	buttonRemapTable[static_cast<int>(Button::F11)] = ImGuiKey_F11;
	buttonRemapTable[static_cast<int>(Button::F12)] = ImGuiKey_F12;
#pragma endregion

	io.IniFilename = nullptr;
	if (args.enableImGuiIni)
	{
		static std::string iniFileName;
		iniFileName = eg::ExeRelPath("ImGui.ini");
		io.IniFilename = iniFileName.c_str();
	}

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	ImGui::StyleColorsDark(&ImGui::GetStyle());

	io.SetClipboardTextFn = [](void* data, const char* text) { eg::SetClipboardText(text); };
	io.GetClipboardTextFn = [](void* data) -> const char*
	{
		static std::string clipboardText;
		clipboardText = eg::GetClipboardText();
		return clipboardText.c_str();
	};

	vertexShader = ShaderModule(eg::ShaderStage::Vertex, ImGui_vs_glsl);
	fragmentShader = ShaderModule(eg::ShaderStage::Fragment, ImGui_fs_glsl);

	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader.shaderModule = vertexShader.Handle();
	pipelineCI.fragmentShader.shaderModule = fragmentShader.Handle();
	pipelineCI.label = "imgui";
	pipelineCI.enableScissorTest = true;
	pipelineCI.colorAttachmentFormats[0] = eg::Format::DefaultColor;
	pipelineCI.depthAttachmentFormat = eg::Format::DefaultDepthStencil;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.setBindModes[1] = eg::BindMode::DescriptorSet;
	pipelineCI.blendStates[0] = eg::AlphaBlend;
	pipelineCI.vertexBindings[0] = { sizeof(ImDrawVert), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, static_cast<uint32_t>(offsetof(ImDrawVert, pos)) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, static_cast<uint32_t>(offsetof(ImDrawVert, uv)) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::UInt8Norm, 4,
		                               static_cast<uint32_t>(offsetof(ImDrawVert, col)) };

	pipeline = eg::Pipeline::Create(pipelineCI);

	// ** Creates the font texture **
	bool fontFound = false;

#ifndef __EMSCRIPTEN__
	static const char* FONT_NAMES[] = {
		"DejaVuSans",
		"DroidSans",
		"Arial",
	};

	for (const char* fontName : FONT_NAMES)
	{
		std::string path = GetFontPathByName(fontName);
		if (!path.empty())
		{
			io.Fonts->AddFontFromFileTTF(path.c_str(), args.fontSize * eg::DisplayScaleFactor());
			fontFound = true;
		}
	}
#endif

	if (!fontFound)
	{
		io.Fonts->AddFontDefault();
		io.FontGlobalScale = eg::DisplayScaleFactor();
	}

	unsigned char* fontTexPixels;
	int fontTexWidth, fontTexHeight;
	io.Fonts->GetTexDataAsRGBA32(&fontTexPixels, &fontTexWidth, &fontTexHeight);

	uint64_t fontTexBytes = fontTexWidth * fontTexHeight * 4;
	eg::Buffer fontUploadBuffer(eg::BufferFlags::CopySrc | eg::BufferFlags::MapWrite, fontTexBytes, nullptr);
	void* fontUploadMem = fontUploadBuffer.Map(0, fontTexBytes);
	std::memcpy(fontUploadMem, fontTexPixels, fontTexBytes);
	fontUploadBuffer.Flush(0, fontTexBytes);

	textureSampler = Sampler(SamplerDescription{
		.wrapU = eg::WrapMode::ClampToEdge,
		.wrapV = eg::WrapMode::ClampToEdge,
	});

	eg::TextureCreateInfo fontTexCreateInfo;
	fontTexCreateInfo.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::ShaderSample;
	fontTexCreateInfo.width = fontTexWidth;
	fontTexCreateInfo.height = fontTexHeight;
	fontTexCreateInfo.format = eg::Format::R8G8B8A8_UNorm;
	fontTexCreateInfo.mipLevels = 1;

	fontTexture = eg::Texture::Create2D(fontTexCreateInfo);
	eg::DC.SetTextureData(
		fontTexture, { 0, 0, 0, ToUnsigned(fontTexWidth), ToUnsigned(fontTexHeight), 1, 0 }, fontUploadBuffer, 0);
	io.Fonts->TexID = MakeImTextureID(fontTexture);

	fontTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	buttonEventListener = new eg::EventListener<eg::ButtonEvent>;

	scaleUniformBuffer =
		eg::Buffer(eg::BufferFlags::UniformBuffer | eg::BufferFlags::CopyDst, sizeof(float) * 2, nullptr);
	scaleUniformBufferDescriptorSet = eg::DescriptorSet(pipeline, 0);
	scaleUniformBufferDescriptorSet.BindUniformBuffer(scaleUniformBuffer, 0);

	detail::imguiBeginFrame = &StartFrame;
	detail::imguiEndFrame = &EndFrame;
}

void Uninitialize()
{
	if (!isInitialized)
		return;
	fontTexture.Destroy();
	vertexShader.Destroy();
	fragmentShader.Destroy();
	pipeline.Destroy();
	vertexBuffer.Destroy();
	indexBuffer.Destroy();
	vertexBufferCapacity = 0;
	indexBufferCapacity = 0;
	delete buttonEventListener;
	buttonEventListener = nullptr;
	detail::imguiBeginFrame = nullptr;
	detail::imguiEndFrame = nullptr;
	ImGui::DestroyContext();
	scaleUniformBuffer.Destroy();
	scaleUniformBufferDescriptorSet.Destroy();
}

EG_ON_SHUTDOWN(Uninitialize);

void StartFrame(float dt)
{
	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize.x = eg::CurrentResolutionX();
	io.DisplaySize.y = eg::CurrentResolutionY();
	io.DeltaTime = dt;
	io.MousePos = ImVec2(eg::CursorPos().x, eg::CursorPos().y);
	io.MouseWheel = eg::InputState::Current().scrollY - eg::InputState::Previous().scrollY;
	io.MouseWheelH = eg::InputState::Current().scrollX - eg::InputState::Previous().scrollX;

	buttonEventListener->ProcessAll(
		[&](const eg::ButtonEvent& event)
		{
			if (ImGuiKey imguiKey = buttonRemapTable[static_cast<int>(event.button)])
			{
				io.AddKeyEvent(imguiKey, event.newState);
			}

			switch (event.button)
			{
			case eg::Button::MouseLeft: io.MouseDown[0] = event.newState; break;
			case eg::Button::MouseRight: io.MouseDown[1] = event.newState; break;
			case eg::Button::MouseMiddle: io.MouseDown[2] = event.newState; break;
			case eg::Button::LeftShift:
			case eg::Button::RightShift: io.KeyShift = event.newState; break;
			case eg::Button::LeftControl:
			case eg::Button::RightControl: io.KeyCtrl = event.newState; break;
			case eg::Button::LeftAlt:
			case eg::Button::RightAlt: io.KeyAlt = event.newState; break;
			default: break;
			}
		});

	if (ImGui::GetIO().WantTextInput)
	{
		eg::TextInputActive();
	}
	if (!eg::InputtedText().empty())
	{
		ImGui::GetIO().AddInputCharactersUTF8(eg::InputtedText().c_str());
	}

	ImGui::NewFrame();
}

void EndFrame()
{
	ImGui::Render();

	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData->TotalIdxCount == 0)
		return;

	ImGuiIO& io = ImGui::GetIO();

	static_assert(sizeof(ImDrawIdx) == sizeof(uint16_t));
	uint64_t verticesBytes = drawData->TotalVtxCount * sizeof(ImDrawVert);
	uint64_t indicesBytes = drawData->TotalIdxCount * sizeof(ImDrawIdx);

	if (vertexBufferCapacity < drawData->TotalVtxCount)
	{
		vertexBufferCapacity = RoundToNextMultiple(drawData->TotalVtxCount, 128);
		vertexBuffer = Buffer(
			eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst, vertexBufferCapacity * sizeof(ImDrawVert),
			nullptr);
	}
	if (indexBufferCapacity < drawData->TotalIdxCount)
	{
		indexBufferCapacity = RoundToNextMultiple(drawData->TotalIdxCount, 128);
		indexBuffer = Buffer(
			eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst, indexBufferCapacity * sizeof(ImDrawIdx), nullptr);
	}

	size_t uploadBufferSize = verticesBytes + indicesBytes;
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(uploadBufferSize);
	char* uploadMem = reinterpret_cast<char*>(uploadBuffer.Map());

	// Writes vertices
	uint64_t vertexCount = 0;
	ImDrawVert* verticesMem = reinterpret_cast<ImDrawVert*>(uploadMem);
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		std::copy_n(cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size, verticesMem + vertexCount);
		vertexCount += cmdList->VtxBuffer.Size;
	}

	// Writes indices
	uint32_t indexCount = 0;
	ImDrawIdx* indicesMem = reinterpret_cast<ImDrawIdx*>(uploadMem + verticesBytes);
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* cmdList = drawData->CmdLists[n];
		std::copy_n(cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size, indicesMem + indexCount);
		indexCount += cmdList->IdxBuffer.Size;
	}

	uploadBuffer.Flush();

	eg::DC.CopyBuffer(uploadBuffer.buffer, vertexBuffer, uploadBuffer.offset, 0, verticesBytes);
	eg::DC.CopyBuffer(uploadBuffer.buffer, indexBuffer, uploadBuffer.offset + verticesBytes, 0, indicesBytes);

	vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	indexBuffer.UsageHint(eg::BufferUsage::IndexBuffer);

	const float scale[] = { 2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y };
	scaleUniformBuffer.DCUpdateData<float>(0, scale);
	scaleUniformBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex);

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	eg::DC.BeginRenderPass(rpBeginInfo);

	eg::DC.BindPipeline(pipeline);

	eg::DC.BindDescriptorSet(scaleUniformBufferDescriptorSet, 0);
	eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
	eg::DC.BindIndexBuffer(eg::IndexType::UInt16, indexBuffer, 0);

	// Renders the command lists
	uint32_t firstIndex = 0;
	uint32_t firstVertex = 0;
	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* commandList = drawData->CmdLists[n];

		for (const ImDrawCmd& drawCommand : commandList->CmdBuffer)
		{
			eg::DC.BindDescriptorSet(reinterpret_cast<eg::DescriptorSetHandle>(drawCommand.TextureId), 1);

			if (drawCommand.UserCallback != nullptr)
			{
				drawCommand.UserCallback(commandList, &drawCommand);
			}
			else
			{
				int scissorX = static_cast<int>(std::max(drawCommand.ClipRect.x, 0.0f));
				int scissorY = static_cast<int>(std::max(io.DisplaySize.y - drawCommand.ClipRect.w, 0.0f));
				int scissorW = static_cast<int>(std::min(drawCommand.ClipRect.z, io.DisplaySize.x) - scissorX);
				int scissorH =
					static_cast<int>(std::min(drawCommand.ClipRect.w, io.DisplaySize.y) - drawCommand.ClipRect.y + 1);
				if (scissorW > 0 && scissorH > 0)
				{
					eg::DC.SetScissor(scissorX, scissorY, scissorW, scissorH);
					eg::DC.DrawIndexed(firstIndex, drawCommand.ElemCount, firstVertex, 0, 1);
				}
			}
			firstIndex += drawCommand.ElemCount;
		}
		firstVertex += commandList->VtxBuffer.Size;
	}

	eg::DC.EndRenderPass();
}
} // namespace eg::imgui
