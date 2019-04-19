#pragma once

#include "../../Inc/Common.hpp"
#include "Core.hpp"
#include "Graphics/Graphics.hpp"
#include "Graphics/Abstraction.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "Graphics/Model.hpp"
#include "Graphics/StdVertex.hpp"
#include "Graphics/PerspectiveProjection.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "Graphics/SpriteFont.hpp"
#include "Graphics/MeshBatch.hpp"
#include "Graphics/PBR/BRDFIntegrationMap.hpp"
#include "Graphics/PBR/IrradianceMapGenerator.hpp"
#include "Graphics/PBR/SPFMapGenerator.hpp"
#include "Graphics/RenderDoc.hpp"
#include "Alloc/LinearAllocator.hpp"
#include "Alloc/DirectAllocator.hpp"
#include "Alloc/ObjectPool.hpp"
#include "Alloc/PoolAllocator.hpp"
#include "Platform/FileSystem.hpp"
#include "Platform/DynamicLibrary.hpp"
#include "BitsetUtils.hpp"
#include "Event.hpp"
#include "Log.hpp"
#include "IOUtils.hpp"
#include "InputState.hpp"
#include "GameController.hpp"
#include "TranslationGizmo.hpp"
#include "MainThreadInvoke.hpp"
#include "Span.hpp"
#include "Rectangle.hpp"
#include "Utils.hpp"
#include "Console.hpp"
#include "TextEdit.hpp"
#include "Ray.hpp"
#include "Sphere.hpp"
#include "AABB.hpp"
#include "Plane.hpp"
#include "Lazy.hpp"
#include "Assets/Asset.hpp"
#include "Assets/AssetLoad.hpp"
#include "Assets/AssetGenerator.hpp"
#include "Assets/ModelAsset.hpp"
#include "Assets/ShaderModule.hpp"
#include "Assets/Texture2DLoader.hpp"
#include "Entity/EntityManager.hpp"
#include "Entity/ECTransform.hpp"
#include "Entity/ECModel.hpp"
#include "Entity/ECInvoke.hpp"
