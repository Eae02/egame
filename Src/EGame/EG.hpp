#pragma once

#include "Alloc/DirectAllocator.hpp"
#include "Alloc/LinearAllocator.hpp"
#include "Alloc/ObjectPool.hpp"
#include "Alloc/PoolAllocator.hpp"
#include "Assert.hpp"
#include "Assets/Asset.hpp"
#include "Assets/AssetGenerator.hpp"
#include "Assets/AssetLoad.hpp"
#include "Assets/ModelAsset.hpp"
#include "Assets/ShaderModule.hpp"
#include "Assets/Texture2DLoader.hpp"
#include "Assets/WebAssetDownload.hpp"
#include "Audio/AudioPlayer.hpp"
#include "BitsetUtils.hpp"
#include "Compression.hpp"
#include "Console.hpp"
#include "Core.hpp"
#include "Event.hpp"
#include "GameController.hpp"
#include "Geometry/AABB.hpp"
#include "Geometry/Collision.hpp"
#include "Geometry/CollisionMesh.hpp"
#include "Geometry/Plane.hpp"
#include "Geometry/Ray.hpp"
#include "Geometry/Rectangle.hpp"
#include "Geometry/Sphere.hpp"
#include "Gizmo/RotationGizmo.hpp"
#include "Gizmo/TranslationGizmo.hpp"
#include "Graphics/Abstraction.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "Graphics/Animation/Animation.hpp"
#include "Graphics/Animation/AnimationDriver.hpp"
#include "Graphics/Animation/BoneMatrixBuffer.hpp"
#include "Graphics/Animation/KeyFrame.hpp"
#include "Graphics/Animation/KeyFrameList.hpp"
#include "Graphics/FullscreenShader.hpp"
#include "Graphics/Graphics.hpp"
#include "Graphics/MeshBatch.hpp"
#include "Graphics/MeshBatchOrdered.hpp"
#include "Graphics/Model.hpp"
#include "Graphics/PBR/BRDFIntegrationMap.hpp"
#include "Graphics/PBR/IrradianceMapGenerator.hpp"
#include "Graphics/PBR/SPFMapGenerator.hpp"
#include "Graphics/Particles/ParticleEmitterInstance.hpp"
#include "Graphics/Particles/ParticleEmitterType.hpp"
#include "Graphics/Particles/ParticleManager.hpp"
#include "Graphics/Particles/Vec3Generator.hpp"
#include "Graphics/PerspectiveProjection.hpp"
#include "Graphics/RenderDoc.hpp"
#include "Graphics/ScreenRenderTexture.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "Graphics/SpriteFont.hpp"
#include "Graphics/StdVertex.hpp"
#include "Hash.hpp"
#include "IOUtils.hpp"
#include "InputState.hpp"
#include "Log.hpp"
#include "MainThreadInvoke.hpp"
#include "Platform/DynamicLibrary.hpp"
#include "Platform/FileSystem.hpp"
#include "Profiling/Profiler.hpp"
#include "Span2.hpp"
#include "String.hpp"
#include "TextEdit.hpp"
#include "Utils.hpp"
