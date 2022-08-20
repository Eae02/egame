#include "EGameImGui.hpp"

#include "../EGame/Core.hpp"
#include "../EGame/InputState.hpp"
#include "../EGame/Event.hpp"
#include "../EGame/Platform/FileSystem.hpp"

#include "../Shaders/Build/ImGui.vs.h"
#include "../Shaders/Build/ImGui.fs.h"

namespace eg::imgui
{
	static bool isInitialized;
	
	static eg::EventListener<eg::ButtonEvent>* buttonEventListener;
	
	static Texture fontTexture;
	
	static ShaderModule vertexShader;
	static ShaderModule fragmentShader;
	static Pipeline pipeline;
	
	static int vertexBufferCapacity;
	static Buffer vertexBuffer;
	
	static int indexBufferCapacity;
	static Buffer indexBuffer;
	
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
		io.KeyMap[ImGuiKey_Tab]        = static_cast<int>(eg::Button::Tab);
		io.KeyMap[ImGuiKey_LeftArrow]  = static_cast<int>(eg::Button::LeftArrow);
		io.KeyMap[ImGuiKey_RightArrow] = static_cast<int>(eg::Button::RightArrow);
		io.KeyMap[ImGuiKey_UpArrow]    = static_cast<int>(eg::Button::UpArrow);
		io.KeyMap[ImGuiKey_DownArrow]  = static_cast<int>(eg::Button::DownArrow);
		io.KeyMap[ImGuiKey_PageUp]     = static_cast<int>(eg::Button::PageUp);
		io.KeyMap[ImGuiKey_PageDown]   = static_cast<int>(eg::Button::PageDown);
		io.KeyMap[ImGuiKey_Home]       = static_cast<int>(eg::Button::Home);
		io.KeyMap[ImGuiKey_End]        = static_cast<int>(eg::Button::End);
		io.KeyMap[ImGuiKey_Delete]     = static_cast<int>(eg::Button::Delete);
		io.KeyMap[ImGuiKey_Backspace]  = static_cast<int>(eg::Button::Backspace);
		io.KeyMap[ImGuiKey_Enter]      = static_cast<int>(eg::Button::Enter);
		io.KeyMap[ImGuiKey_Escape]     = static_cast<int>(eg::Button::Escape);
		io.KeyMap[ImGuiKey_Space]      = static_cast<int>(eg::Button::Space);
		io.KeyMap[ImGuiKey_A]          = static_cast<int>(eg::Button::A);
		io.KeyMap[ImGuiKey_C]          = static_cast<int>(eg::Button::C);
		io.KeyMap[ImGuiKey_V]          = static_cast<int>(eg::Button::V);
		io.KeyMap[ImGuiKey_X]          = static_cast<int>(eg::Button::X);
		io.KeyMap[ImGuiKey_Y]          = static_cast<int>(eg::Button::Y);
		io.KeyMap[ImGuiKey_Z]          = static_cast<int>(eg::Button::Z);
		
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
		
		io.SetClipboardTextFn = [] (void* data, const char* text) { eg::SetClipboardText(text); };
		io.GetClipboardTextFn = [] (void* data) -> const char* {
			static std::string clipboardText;
			clipboardText = eg::GetClipboardText();
			return clipboardText.c_str();
		};
		
		vertexShader = ShaderModule(eg::ShaderStage::Vertex, ImGui_vs_glsl);
		fragmentShader = ShaderModule(eg::ShaderStage::Fragment, ImGui_fs_glsl);
		
		eg::GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = vertexShader.Handle();
		pipelineCI.fragmentShader = fragmentShader.Handle();
		pipelineCI.enableScissorTest = true;
		pipelineCI.blendStates[0] = eg::AlphaBlend;
		pipelineCI.vertexBindings[0] = { sizeof(ImDrawVert), eg::InputRate::Vertex };
		pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, static_cast<uint32_t>(offsetof(ImDrawVert, pos)) };
		pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, static_cast<uint32_t>(offsetof(ImDrawVert, uv)) };
		pipelineCI.vertexAttributes[2] = { 0, eg::DataType::UInt8Norm, 4, static_cast<uint32_t>(offsetof(ImDrawVert, col)) };
		
		pipeline = eg::Pipeline::Create(pipelineCI);
		pipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
		
		// ** Creates the font texture **
#ifdef __EMSCRIPTEN__
		io.Fonts->AddFontDefault();
#else
		const char* fontPaths[] = 
		{
			args.fontPath,
#if defined(__linux__)
			"/usr/share/fonts/TTF/DejaVuSans.ttf",
			"/usr/share/fonts/TTF/DroidSans.ttf",
			"/usr/share/fonts/droid/DroidSans.ttf",
			"/usr/share/fonts/TTF/arial.ttf"
#elif defined(_WIN32)
			"C:\\Windows\\Fonts\\arial.ttf"
#endif
		};
		
		auto fontIt = std::find_if(std::begin(fontPaths), std::end(fontPaths), [] (const char* path)
		{
			return path != nullptr && eg::FileExists(path);
		});
		
		if (fontIt != std::end(fontPaths))
			io.Fonts->AddFontFromFileTTF(*fontIt, args.fontSize);
		else
			io.Fonts->AddFontDefault();
#endif
		
		unsigned char* fontTexPixels;
		int fontTexWidth, fontTexHeight;
		io.Fonts->GetTexDataAsRGBA32(&fontTexPixels, &fontTexWidth, &fontTexHeight);
		
		uint64_t fontTexBytes = fontTexWidth * fontTexHeight * 4;
		eg::Buffer fontUploadBuffer(
			eg::BufferFlags::CopySrc | eg::BufferFlags::HostAllocate | eg::BufferFlags::MapWrite,
			fontTexBytes, nullptr);
		void* fontUploadMem = fontUploadBuffer.Map(0, fontTexBytes);
		std::memcpy(fontUploadMem, fontTexPixels, fontTexBytes);
		fontUploadBuffer.Flush(0, fontTexBytes);
		
		eg::SamplerDescription fontTexSampler;
		fontTexSampler.wrapU = eg::WrapMode::ClampToEdge;
		fontTexSampler.wrapV = eg::WrapMode::ClampToEdge;
		
		eg::TextureCreateInfo fontTexCreateInfo;
		fontTexCreateInfo.flags = eg::TextureFlags::CopyDst | eg::TextureFlags::ShaderSample;
		fontTexCreateInfo.width = fontTexWidth;
		fontTexCreateInfo.height = fontTexHeight;
		fontTexCreateInfo.format = eg::Format::R8G8B8A8_UNorm;
		fontTexCreateInfo.defaultSamplerDescription = &fontTexSampler;
		fontTexCreateInfo.mipLevels = 1;
		
		fontTexture = eg::Texture::Create2D(fontTexCreateInfo);
		eg::DC.SetTextureData(fontTexture, { 0, 0, 0, ToUnsigned(fontTexWidth), ToUnsigned(fontTexHeight), 1, 0 }, fontUploadBuffer, 0);
		io.Fonts->TexID = MakeImTextureID(fontTexture.GetView());
		
		fontTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
		
		buttonEventListener = new eg::EventListener<eg::ButtonEvent>;
		
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
		
		buttonEventListener->ProcessAll([&] (const eg::ButtonEvent& event)
		{
			io.KeysDown[static_cast<int>(event.button)] = event.newState;
			
			switch (event.button)
			{
			case eg::Button::MouseLeft:
				io.MouseDown[0] = event.newState;
				break;
			case eg::Button::MouseRight:
				io.MouseDown[1] = event.newState;
				break;
			case eg::Button::MouseMiddle:
				io.MouseDown[2] = event.newState;
				break;
			case eg::Button::LeftShift:
			case eg::Button::RightShift:
				io.KeyShift = event.newState;
				break;
			case eg::Button::LeftControl:
			case eg::Button::RightControl:
				io.KeyCtrl = event.newState;
				break;
			case eg::Button::LeftAlt:
			case eg::Button::RightAlt:
				io.KeyAlt = event.newState;
				break;
			default:
				break;
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
			vertexBuffer = Buffer(eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst, vertexBufferCapacity * sizeof(ImDrawVert), nullptr);
		}
		if (indexBufferCapacity < drawData->TotalIdxCount)
		{
			indexBufferCapacity = RoundToNextMultiple(drawData->TotalIdxCount, 128);
			indexBuffer = Buffer(eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst, indexBufferCapacity * sizeof(ImDrawIdx), nullptr);
		}
		
		size_t uploadBufferSize = verticesBytes + indicesBytes;
		eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(uploadBufferSize);
		char* uploadMem = reinterpret_cast<char*>(uploadBuffer.Map());
		
		//Writes vertices
		uint64_t vertexCount = 0;
		ImDrawVert* verticesMem = reinterpret_cast<ImDrawVert*>(uploadMem);
		for (int n = 0; n < drawData->CmdListsCount; n++)
		{
			const ImDrawList* cmdList = drawData->CmdLists[n];
			std::copy_n(cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size, verticesMem + vertexCount);
			vertexCount += cmdList->VtxBuffer.Size;
		}
		
		//Writes indices
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
		
		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
		rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
		eg::DC.BeginRenderPass(rpBeginInfo);
		
		eg::DC.BindPipeline(pipeline);
		
		float scale[] = { 2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y };
		eg::DC.PushConstants(0, scale);
		
		eg::DC.BindVertexBuffer(0, vertexBuffer, 0);
		eg::DC.BindIndexBuffer(eg::IndexType::UInt16, indexBuffer, 0);
		
		//Renders the command lists
		uint32_t firstIndex = 0;
		uint32_t firstVertex = 0;
		for (int n = 0; n < drawData->CmdListsCount; n++)
		{
			const ImDrawList* commandList = drawData->CmdLists[n];
			
			for (const ImDrawCmd& drawCommand : commandList->CmdBuffer)
			{
				eg::DC.BindTextureView(reinterpret_cast<eg::TextureViewHandle>(drawCommand.TextureId), 0, 0);
				
				if (drawCommand.UserCallback != nullptr)
				{
					drawCommand.UserCallback(commandList, &drawCommand);
				}
				else
				{
					int scissorX = static_cast<int>(std::max(drawCommand.ClipRect.x, 0.0f));
					int scissorY = static_cast<int>(std::max(io.DisplaySize.y - drawCommand.ClipRect.w, 0.0f));
					int scissorW = static_cast<int>(std::min(drawCommand.ClipRect.z, io.DisplaySize.x) - scissorX);
					int scissorH = static_cast<int>(std::min(drawCommand.ClipRect.w, io.DisplaySize.y) - drawCommand.ClipRect.y + 1);
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
}
