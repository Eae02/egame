#pragma once

#include "ParticleEmitterType.hpp"
#include "../AbstractionHL.hpp"
#include "../../API.hpp"
#include "../../Frustum.hpp"

#include <thread>
#include <condition_variable>
#include <chrono>
#include <emmintrin.h>

namespace eg
{
#pragma pack(push, 1)
	struct ParticleInstance
	{
		float position[3];
		float size;
		uint16_t texCoord[4];
		uint8_t sinR;
		uint8_t cosR;
		uint8_t opacity;
		uint8_t additiveBlend;
	};
#pragma pack(pop)
	
	class EG_API ParticleManager
	{
	public:
		friend class ParticleEmitterInstance;
		
		ParticleManager();
		
		~ParticleManager();
		
		ParticleManager(ParticleManager&&) = delete;
		ParticleManager(const ParticleManager&) = delete;
		ParticleManager& operator=(ParticleManager&&) = delete;
		ParticleManager& operator=(const ParticleManager&) = delete;
		
		void Step(float dt, const Frustum& frustum, const glm::vec3& cameraForward);
		
		uint32_t ParticlesToDraw() const
		{
			return m_instancesToDraw;
		}
		
		BufferRef ParticlesBuffer() const
		{
			return m_deviceBuffer;
		}
		
		void SetGravity(glm::vec3 gravity)
		{
			m_gravity[0] = gravity.x;
			m_gravity[1] = gravity.y;
			m_gravity[2] = gravity.z;
			m_gravity[3] = 0.0f;
		}
		
		void SetTextureSize(int width, int height)
		{
			m_textureWidth = width;
			m_textureHeight = height;
		}
		
		class ParticleEmitterInstance AddEmitter(const ParticleEmitterType& type);
		
	private:
		struct Emitter
		{
			uint32_t id;
			bool alive;
			bool hasSetTransform;
			const ParticleEmitterType* type;
			float timeSinceEmit;
			float emissionDelay;
			glm::vec3 gravity;
			glm::mat4 transform;
			glm::mat4 prevTransform;
			
			void UpdateEmissionDelay(float rateFactor)
			{
				emissionDelay = 1.0f / (type->emissionRate * rateFactor);
			}
		};
		
		Emitter& GetEmitter(uint32_t id);
		
		void ThreadTarget();
		
		static constexpr size_t PARTICLES_PER_PAGE = 1024;
		
		struct ParticlePage
		{
			const ParticleEmitterType* emitterType;
			uint32_t livingParticles;
			__m128 position[PARTICLES_PER_PAGE];
			__m128 velocity[PARTICLES_PER_PAGE];
			uint8_t textureVariants[PARTICLES_PER_PAGE];
			alignas(16) float lifeProgress[PARTICLES_PER_PAGE];
			alignas(16) float oneOverLifeTime[PARTICLES_PER_PAGE];
			alignas(16) float rotation[PARTICLES_PER_PAGE];
			alignas(16) float angularVelocity[PARTICLES_PER_PAGE];
			alignas(16) float initialOpacity[PARTICLES_PER_PAGE];
			alignas(16) float deltaOpacity[PARTICLES_PER_PAGE];
			alignas(16) float currentOpacity[PARTICLES_PER_PAGE];
			alignas(16) float initialSize[PARTICLES_PER_PAGE];
			alignas(16) float deltaSize[PARTICLES_PER_PAGE];
			alignas(16) float currentSize[PARTICLES_PER_PAGE];
		};
		
		static constexpr size_t PARTICLES_PER_UPLOAD_BUFFER = 16384;
		
		struct ParticleUploadBuffer
		{
			Buffer buffer;
			ParticleInstance* instances;
			int reuseDelay;
			int instancesWritten;
		};
		std::vector<ParticleUploadBuffer> m_particleUploadBuffers;
		int m_missingUploadBuffers = 0;
		
		uint32_t m_deviceBufferCapacity = 0;
		Buffer m_deviceBuffer;
		
		uint32_t m_instancesToDraw = 0;
		
		void AddUploadBuffer();
		
		ParticlePage* GetPage(const ParticleEmitterType& emitterType);
		
		std::vector<std::unique_ptr<ParticlePage>> m_pagesKeepAlive;
		std::vector<ParticlePage*> m_pages;
		std::vector<ParticlePage*> m_emptyPages;
		
		uint32_t m_nextEmitterId = 0;
		std::vector<Emitter> m_btEmitters;
		std::vector<Emitter> m_mtEmitters;
		
		float m_currentTime = 0;
		__m128 m_frustumPlanes[6];
		__m128 m_cameraForward;
		
		__m128 m_gravity;
		
		int m_textureWidth = 1;
		int m_textureHeight = 1;
		
		std::mutex m_mutex;
		
		std::condition_variable m_simulationDoneSignal;
		std::condition_variable m_stepDoneSignal;
		
		enum class State
		{
			Simulate,
			SimulationDone,
			Stop
		};
		State m_state = State::Simulate;
		
		std::mt19937 m_random;
		
		std::thread m_thread;
	};
}