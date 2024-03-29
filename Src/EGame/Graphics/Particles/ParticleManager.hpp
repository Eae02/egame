#pragma once

#include "../../API.hpp"
#include "../../Geometry/Frustum.hpp"
#include "../../SIMD.hpp"
#include "../AbstractionHL.hpp"
#include "ParticleEmitterType.hpp"

#include <condition_variable>
#include <thread>

namespace eg
{
struct __attribute__((__packed__)) ParticleInstance
{
	float position[3];
	float size;
	uint16_t texCoord[4];
	uint8_t sinR;
	uint8_t cosR;
	uint8_t opacity;
	uint8_t additiveBlend;
};

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

	uint32_t ParticlesToDraw() const { return m_instancesToDraw; }

	BufferRef ParticlesBuffer() const { return m_deviceBuffer; }

	void SetGravity(glm::vec3 gravity);

	void SetTextureSize(int width, int height);

	class ParticleEmitterInstance AddEmitter(const ParticleEmitterType& type);

private:
	struct Emitter
	{
		uint32_t id;
		bool alive;
		bool hasSetTransform;
		bool hasSetOldTransform;
		const ParticleEmitterType* type;
		float timeSinceEmit;
		float emissionDelay;
		glm::vec3 gravity;
		glm::mat4 transform;
		glm::mat4 prevTransform;

		void UpdateEmissionDelay(float rateFactor) { emissionDelay = 1.0f / (type->emissionRate * rateFactor); }
	};

	Emitter& GetEmitter(uint32_t id);

	void SimulateOneStep();

	static constexpr size_t PARTICLES_PER_PAGE = 1024;

	struct ParticlePage
	{
		const ParticleEmitterType* emitterType;
		uint32_t livingParticles;
		m128 position[PARTICLES_PER_PAGE];
		m128 velocity[PARTICLES_PER_PAGE];
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
	size_t m_missingUploadBuffers = 0;

	uint32_t m_deviceBufferCapacity = 0;
	Buffer m_deviceBuffer;

	uint32_t m_instancesToDraw = 0;

	void AddUploadBuffer();

	ParticlePage* GetPage(const ParticleEmitterType& emitterType);

	std::vector<std::unique_ptr<ParticlePage>> m_pagesKeepAlive;
	std::vector<ParticlePage*> m_pages;
	std::vector<ParticlePage*> m_emptyPages;

	std::vector<ParticleInstance> m_particleInstances;
	std::vector<std::pair<float, int>> m_particleDepths;

	uint32_t m_nextEmitterId = 0;
	std::vector<Emitter> m_btEmitters;
	std::vector<Emitter> m_mtEmitters;

	float m_currentTime = 0;
	float m_lastSimTime = 0;
	m128 m_frustumPlanes[6];
	m128 m_cameraForward;

	m128 m_gravity;

	glm::vec2 m_texturePixelSize;

	std::mt19937 m_random;

#ifndef __EMSCRIPTEN__
	void ThreadTarget();

	enum class State
	{
		Simulate,
		SimulationDone,
		Stop
	};
	State m_state = State::Simulate;

	std::mutex m_mutex;

	std::condition_variable m_simulationDoneSignal;
	std::condition_variable m_stepDoneSignal;

	std::thread m_thread;
#endif
};
} // namespace eg
