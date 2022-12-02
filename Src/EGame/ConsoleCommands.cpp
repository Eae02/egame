#include "ConsoleCommands.hpp"
#include "Console.hpp"
#include "Core.hpp"
#include "Platform/Debug.hpp"
#include "Profiling/ProfilerPane.hpp"
#include "Graphics/Model.hpp"

#include <iomanip>

namespace eg::detail
{
	void RegisterConsoleCommands()
	{
		console::AddCommand("enableProfiling", 0, [&] (std::span<const std::string_view> args, console::Writer& writer)
		{
			if (!EnableProfiling())
				writer.WriteLine(console::InfoColor, "Profiling already enabled");
		});
		
		console::AddCommand("ppane", 0, [&] (std::span<const std::string_view> args, console::Writer& writer)
		{
			EnableProfiling();
			bool visible = !ProfilerPane::Instance()->visible;
			if (args.size() == 1)
			{
				if (args[0] == "show")
					visible = true;
				else if (args[0] == "hide")
					visible = false;
				else
				{
					writer.WriteLine(console::ErrorColor, "Invalid argument to ppane, should be 'show' or 'hide'");
					return;
				}
			}
			ProfilerPane::Instance()->visible = visible;
		});
		
		console::AddCommand("modelInfo", 1, [&] (std::span<const std::string_view> args, console::Writer& writer)
		{
			const Model* model = eg::FindAsset<Model>(args[0]);
			if (model == nullptr)
			{
				writer.Write(console::ErrorColor, "The model ");
				writer.Write(console::ErrorColor.ScaleRGB(1.5f), args[0]);
				writer.WriteLine(console::ErrorColor, " doesn't exist");
				return;
			}
			
			writer.Write(console::InfoColor, "Information about ");
			writer.Write(console::InfoColorSpecial, args[0]);
			writer.WriteLine(console::InfoColor, ":");
			
			
			writer.Write(console::InfoColor, "  vtype:");
			std::string demangledTypeName = DemangeTypeName(model->VertexType().name());
			writer.Write(console::InfoColorSpecial, demangledTypeName);
			writer.Write(console::InfoColor, " itype:");
			writer.WriteLine(console::InfoColorSpecial, model->IndexType() == IndexType::UInt32 ? "uint32" : "uint16");
			
			//Prepares column data
			size_t nameColLen = 0;
			size_t vertexColLen = 0;
			size_t triangleColLen = 0;
			std::vector<std::string> vertexStrings(model->NumMeshes());
			std::vector<std::string> triangleStrings(model->NumMeshes());
			for (size_t i = 0; i < model->NumMeshes(); i++)
			{
				vertexStrings[i] = std::to_string(model->GetMesh(i).numVertices);
				triangleStrings[i] = std::to_string(model->GetMesh(i).numIndices / 3);
				
				nameColLen = std::max(nameColLen, model->GetMesh(i).name.size());
				vertexColLen = std::max(vertexColLen, vertexStrings[i].size());
				triangleColLen = std::max(triangleColLen, triangleStrings[i].size());
			}
			
			//Writes information about meshes
			std::array<const char*, 3> meshAccessNames = { "gpu", "cpu", "gpu+cpu" };
			uint32_t totVertices = 0;
			uint32_t totIndices = 0;
			for (size_t i = 0; i < model->NumMeshes(); i++)
			{
				std::string str = "  mesh[" + std::to_string(i) + "] '";
				writer.Write(console::InfoColor, str);
				writer.Write(console::InfoColorSpecial, model->GetMesh(i).name);
				
				str = "'" + std::string(nameColLen + 1 - model->GetMesh(i).name.size(), ' ') + "V:";
				writer.Write(console::InfoColor, str);
				writer.Write(console::InfoColorSpecial, vertexStrings[i]);
				
				str = std::string(vertexColLen + 1 - vertexStrings[i].size(), ' ') + "T:";
				writer.Write(console::InfoColor, str);
				writer.Write(console::InfoColorSpecial, triangleStrings[i]);
				
				str = std::string(triangleColLen + 1 - triangleStrings[i].size(), ' ') + "A:";
				writer.Write(console::InfoColor, str);
				writer.WriteLine(console::InfoColorSpecial, meshAccessNames.at(static_cast<int>(model->GetMesh(i).access)));
				
				totVertices += model->GetMesh(i).numVertices;
				totIndices += model->GetMesh(i).numIndices;
			}
			
			//Writes information about materials
			for (size_t i = 0; i < model->NumMaterials(); i++)
			{
				std::string str = "  mat[" + std::to_string(i) + "] '";
				writer.Write(console::InfoColor, str);
				writer.Write(console::InfoColorSpecial, model->GetMaterialName(i));
				writer.WriteLine(console::InfoColor, "'");
			}
			
			//Writes information about animations
			for (size_t i = 0; i < model->Animations().size(); i++)
			{
				std::string str = "  anim[" + std::to_string(i) + "] '";
				writer.Write(console::InfoColor, str);
				writer.Write(console::InfoColorSpecial, model->Animations()[i].name);
				writer.WriteLine(console::InfoColor, "'");
			}
			
			writer.Write(console::InfoColor, "  total vertices: ");
			std::string totalVerticesStr = std::to_string(totVertices);
			writer.WriteLine(console::InfoColorSpecial, totalVerticesStr);
			
			writer.Write(console::InfoColor, "  total triangles: ");
			std::string totalTrianglesStr = std::to_string(totIndices / 3);
			writer.WriteLine(console::InfoColorSpecial, totalTrianglesStr);
			
			if (!model->skeleton.Empty())
			{
				writer.Write(console::InfoColor, "  total bones: ");
				std::string totalBonesStr = std::to_string(model->skeleton.NumBones());
				writer.WriteLine(console::InfoColorSpecial, totalBonesStr);
			}
		});
		
		console::SetCompletionProvider("modelInfo", 0, [] (std::span<const std::string_view> args, eg::console::CompletionsList& list)
		{
			std::type_index typeIndex(typeid(Model));
			AssetCommandCompletionProvider(list, &typeIndex);
		});
		
		console::AddCommand("gmem", 0, [&] (std::span<const std::string_view> args, console::Writer& writer)
		{
			if (gal::GetMemoryStat == nullptr)
			{
				writer.WriteLine(console::WarnColor, "gmem is not supported by this graphics API");
			}
			else
			{
				GraphicsMemoryStat memStat = gal::GetMemoryStat();
				
				std::ostringstream amountUsedStream;
				amountUsedStream << std::setprecision(2) << std::fixed <<
					(static_cast<double>(memStat.allocatedBytes) / (1024.0 * 1024.0));
				std::string amountUsedString = amountUsedStream.str();
				
				writer.Write(console::InfoColor, "Graphics memory info: ");
				writer.Write(console::InfoColorSpecial, amountUsedString);
				writer.Write(console::InfoColor, " MiB in use, ");
				writer.Write(console::InfoColorSpecial, std::to_string(memStat.numBlocks));
				writer.Write(console::InfoColor, " blocks, ");
				writer.Write(console::InfoColorSpecial, std::to_string(memStat.unusedRanges));
				writer.Write(console::InfoColor, " unused ranges");
			}
		});
		
		console::AddCommand("gpuinfo", 0, [&] (std::span<const std::string_view> args, console::Writer& writer)
		{
			writer.Write(console::InfoColor, "GPU Name:   ");
			writer.WriteLine(console::InfoColorSpecial, GetGraphicsDeviceInfo().deviceName);
			writer.Write(console::InfoColor, "GPU Vendor: ");
			writer.WriteLine(console::InfoColorSpecial, GetGraphicsDeviceInfo().deviceVendorName);
		});
	}
}
