#include "ParticleManager.hpp"
#include "ParticleEmitterInstance.hpp"

#include <bitset>
#include <ctime>
#include <smmintrin.h>

namespace eg
{
	ParticleManager::ParticleManager()
		: m_random(std::time(nullptr)), m_thread(&ParticleManager::ThreadTarget, this)
	{
		SetGravity(glm::vec3(0, -5, 0));
	}
	
	ParticleManager::~ParticleManager()
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_state = State::Stop;
		}
		m_stepDoneSignal.notify_one();
		m_thread.join();
	}
	
	void ParticleManager::ThreadTarget()
	{
		std::vector<ParticleInstance> particleInstances;
		std::vector<std::pair<float, int>> particleDepths;
		
		float lastSimTime = m_currentTime;
		while (true)
		{
			float dt = m_currentTime - lastSimTime;
			lastSimTime = m_currentTime;
			
			__m128 dt4 = _mm_set1_ps(dt);
			
			int particleCount = 0;
			
			//Updates existing particles
			for (ParticlePage* page : m_pages)
			{
				int numAlive = page->livingParticles;
				int numAliveDiv4 = (page->livingParticles + 3) / 4;
				
				int numDead = 0;
				int deadIndices[PARTICLES_PER_PAGE];
				
				__m128* rotationM = reinterpret_cast<__m128*>(page->rotation);
				__m128* angularVelocityM = reinterpret_cast<__m128*>(page->angularVelocity);
				__m128* lifeProgressM = reinterpret_cast<__m128*>(page->lifeProgress);
				__m128* oneOverLifeTimeM = reinterpret_cast<__m128*>(page->oneOverLifeTime);
				
				//Increases life progress
				for (int i = numAliveDiv4 - 1; i >= 0; i--)
				{
					lifeProgressM[i] = _mm_add_ps(lifeProgressM[i], _mm_mul_ps(oneOverLifeTimeM[i], dt4));
					
					alignas(16) float lp[4];
					_mm_store_ps(lp, lifeProgressM[i]);
					
					for (int j = 3; j >= 0; j--)
					{
						if (lp[j] > 1.0f)
						{
							deadIndices[numDead++] = i * 4 + j;
						}
					}
				}
				
				//Removes dead particles
				for (int i = 0; i < numDead; i++)
				{
					int idx = deadIndices[i];
					if (idx >= (int)page->livingParticles)
						continue;
					
					numAlive--;
					page->position[idx] = page->position[numAlive];
					page->velocity[idx] = page->velocity[numAlive];
					page->textureVariants[idx] = page->textureVariants[numAlive];
					page->lifeProgress[idx] = page->lifeProgress[numAlive];
					page->oneOverLifeTime[idx] = page->oneOverLifeTime[numAlive];
					page->rotation[idx] = page->rotation[numAlive];
					page->angularVelocity[idx] = page->angularVelocity[numAlive];
					page->initialOpacity[idx] = page->initialOpacity[numAlive];
					page->deltaOpacity[idx] = page->deltaOpacity[numAlive];
					page->initialSize[idx] = page->initialSize[numAlive];
					page->deltaSize[idx] = page->deltaSize[numAlive];
				}
				page->livingParticles = numAlive;
				numAliveDiv4 = (numAlive + 3) / 4;
				
				//Updates rotation and writes current size and opacity
				__m128* initialOpacityM = reinterpret_cast<__m128*>(page->initialOpacity);
				__m128* deltaOpacityM = reinterpret_cast<__m128*>(page->deltaOpacity);
				__m128* currentOpacityM = reinterpret_cast<__m128*>(page->currentOpacity);
				__m128* initialSizeM = reinterpret_cast<__m128*>(page->initialSize);
				__m128* deltaSizeM = reinterpret_cast<__m128*>(page->deltaSize);
				__m128* currentSizeM = reinterpret_cast<__m128*>(page->currentSize);
				for (int i = 0; i < numAliveDiv4; i++)
				{
					rotationM[i] = _mm_add_ps(rotationM[i], _mm_mul_ps(angularVelocityM[i], dt4));
					currentOpacityM[i] = _mm_mul_ps(
						_mm_add_ps(initialOpacityM[i], _mm_mul_ps(deltaOpacityM[i], lifeProgressM[i])),
						_mm_set1_ps(255.0f));
					currentSizeM[i] = _mm_add_ps(initialSizeM[i], _mm_mul_ps(deltaSizeM[i], lifeProgressM[i]));
				}
				
				//Updates velocity and position
				__m128 deltaVelGravity = _mm_mul_ps(m_gravity, _mm_set1_ps(dt * page->emitterType->gravity));
				__m128 deltaVelDragFactor = _mm_set1_ps(-dt * page->emitterType->drag);
				for (int i = 0; i < numAlive; i++)
				{
					__m128 vel = page->velocity[i];
					vel = _mm_add_ps(vel, _mm_mul_ps(deltaVelDragFactor, vel));
					vel = _mm_add_ps(vel, deltaVelGravity);
					page->position[i] = _mm_add_ps(page->position[i], _mm_mul_ps(vel, dt4));
					page->velocity[i] = vel;
				}
				
				particleCount += numAlive;
			}
			
			for (int i = m_pages.size() - 1; i >= 0; i--)
			{
				if (m_pages[i]->livingParticles == 0)
				{
					m_emptyPages.push_back(m_pages[i]);
					m_pages[i] = m_pages.back();
					m_pages.pop_back();
				}
			}
			
			//Spawns new particles
			for (Emitter& emitter : m_btEmitters)
			{
				float oldTSE = emitter.timeSinceEmit;
				emitter.timeSinceEmit += dt;
				
				int emissionsMade = 1;
				while (emitter.timeSinceEmit > emitter.emissionDelay)
				{
					ParticlePage* page = GetPage(*emitter.type);
					EG_ASSERT(page->livingParticles < PARTICLES_PER_PAGE);
					
					auto Vec3GenVisitor = [&] (const auto& generator) { return generator(m_random); };
					
					uint32_t idx = page->livingParticles++;
					
					float transformIA = glm::clamp((emissionsMade * emitter.emissionDelay - oldTSE) / dt, 0.0f, 1.0f);
					auto TransformV3 = [&] (const glm::vec3& v, float w)
					{
						glm::vec3 prev(emitter.prevTransform * glm::vec4(v, w));
						glm::vec3 next(emitter.transform * glm::vec4(v, w));
						return glm::mix(prev, next, transformIA);
					};
					
					glm::vec4 position(TransformV3(std::visit(Vec3GenVisitor, emitter.type->positionGenerator), 1), 1);
					glm::vec4 velocity(TransformV3(std::visit(Vec3GenVisitor, emitter.type->velocityGenerator), 0), 0);
					
					page->position[idx] = _mm_loadu_ps(&position.x);
					page->velocity[idx] = _mm_loadu_ps(&velocity.x);
					
					page->oneOverLifeTime[idx] = 1.0f / emitter.type->lifeTime(m_random);
					page->lifeProgress[idx] = 0;
					page->rotation[idx] = emitter.type->initialRotation(m_random);
					page->angularVelocity[idx] = emitter.type->angularVelocity(m_random);
					
					page->initialOpacity[idx] = emitter.type->initialOpacity(m_random);
					float finalOpacity = emitter.type->finalOpacity(m_random) * page->initialOpacity[idx];
					page->deltaOpacity[idx] = finalOpacity - page->initialOpacity[idx];
					
					page->initialSize[idx] = emitter.type->initialSize(m_random);
					float finalSize = emitter.type->finalSize(m_random) * page->initialSize[idx];
					page->deltaSize[idx] = finalSize - page->initialSize[idx];
					
					emitter.timeSinceEmit -= emitter.emissionDelay;
					emissionsMade++;
				}
				
				emitter.prevTransform = emitter.transform;
			}
			
			float texCoordScaleX = (float)UINT16_MAX / (float)m_textureWidth;
			float texCoordScaleY = (float)UINT16_MAX / (float)m_textureHeight;
			
			//Writes visible particles to the particle instances list
			particleInstances.clear();
			particleDepths.clear();
			for (ParticlePage* page : m_pages)
			{
				for (int i = page->livingParticles - 1; i >= 0; i--)
				{
					bool draw = true;
					for (int p = 0; p < 6; p++)
					{
						float dst = _mm_cvtss_f32(_mm_dp_ps(page->position[i], m_frustumPlanes[p], 0xFF));
						if (dst < -page->currentSize[i])
						{
							draw = false;
							break;
						}
					}
					
					if (draw)
					{
						float depth = _mm_cvtss_f32(_mm_dp_ps(page->position[i], m_cameraForward, 0xFF));
						particleDepths.emplace_back(depth, particleInstances.size());
						
						ParticleInstance& instance = particleInstances.emplace_back();
						for (int j = 0; j < 3; j++)
							instance.position[j] = page->position[i][j];
						instance.size = page->currentSize[i];
						instance.opacity = glm::clamp(page->currentOpacity[i], 0.0f, 255.0f);
						instance.additiveBlend = HasFlag(page->emitterType->flags, ParticleFlags::BlendAdditive) ? 0xFF : 0;
						instance.sinR = (std::sin(page->rotation[i]) + 1.0f) * 127.0f;
						instance.cosR = (std::cos(page->rotation[i]) + 1.0f) * 127.0f;
						
						auto texVariant = page->emitterType->textureVariants[page->textureVariants[i]];
						int frame = std::min<int>(page->lifeProgress[i] * texVariant.numFrames, texVariant.numFrames - 1);
						int texX = texVariant.x + frame * texVariant.width;
						
						instance.texCoord[0] = std::ceil(texX * texCoordScaleX);
						instance.texCoord[1] = std::ceil(texVariant.y * texCoordScaleY);
						instance.texCoord[2] = std::floor((texX + texVariant.width) * texCoordScaleX);
						instance.texCoord[3] = std::floor((texVariant.y + texVariant.height) * texCoordScaleY);
					}
				}
			}
			
			std::sort(particleDepths.begin(), particleDepths.end(), std::greater<>());
			
			for (ParticleUploadBuffer& buffer : m_particleUploadBuffers)
			{
				buffer.reuseDelay = std::max(buffer.reuseDelay - 1, 0);
			}
			
			//Writes particles to upload buffers
			m_missingUploadBuffers = 0;
			size_t uploadBufferIdx = 0;
			for (size_t i = 0; i < particleInstances.size(); i++)
			{
				while (uploadBufferIdx < m_particleUploadBuffers.size() &&
					(m_particleUploadBuffers[uploadBufferIdx].reuseDelay != 0 ||
					 m_particleUploadBuffers[uploadBufferIdx].instancesWritten == PARTICLES_PER_UPLOAD_BUFFER))
				{
					uploadBufferIdx++;
				}
				
				if (uploadBufferIdx == m_particleUploadBuffers.size())
				{
					if (!GetGraphicsDeviceInfo().concurrentResourceCreation)
					{
						m_missingUploadBuffers = (particleInstances.size() - i + PARTICLES_PER_UPLOAD_BUFFER - 1)
							/ PARTICLES_PER_UPLOAD_BUFFER;
						break;
					}
					AddUploadBuffer();
				}
				
				size_t pos = m_particleUploadBuffers[uploadBufferIdx].instancesWritten++;
				m_particleUploadBuffers[uploadBufferIdx].instances[pos] = particleInstances[particleDepths[i].second];
			}
			
			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_state == State::Stop)
				break;
			m_state = State::SimulationDone;
			m_simulationDoneSignal.notify_one();
			m_stepDoneSignal.wait(lock, [&] { return m_state != State::SimulationDone; });
			if (m_state == State::Stop)
				break;
		}
	}
	
	void ParticleManager::AddUploadBuffer()
	{
		ParticleUploadBuffer& buffer = m_particleUploadBuffers.emplace_back();
		
		BufferCreateInfo createInfo;
		createInfo.flags = BufferFlags::CopySrc | BufferFlags::HostAllocate | BufferFlags::MapWrite;
		createInfo.size = PARTICLES_PER_UPLOAD_BUFFER * sizeof(ParticleInstance);
		createInfo.initialData = nullptr;
		createInfo.label = "Particle Upload Buffer";
		buffer.buffer = Buffer(createInfo);
		
		buffer.reuseDelay = 0;
		buffer.instancesWritten = 0;
		buffer.instances = static_cast<ParticleInstance*>(buffer.buffer.Map(0, createInfo.size));
	}
	
	ParticleManager::ParticlePage* ParticleManager::GetPage(const ParticleEmitterType& emitterType)
	{
		auto it = std::lower_bound(m_pages.begin(), m_pages.end(), &emitterType,
			[&] (const ParticlePage* a, const ParticleEmitterType* b) { return a->emitterType < b; });
		
		while (it != m_pages.end() &&
			(*it)->emitterType == &emitterType && (*it)->livingParticles >= PARTICLES_PER_PAGE)
		{
			++it;
		}
		
		if (it != m_pages.end() && (*it)->emitterType == &emitterType)
			return *it;
		
		ParticlePage* page;
		if (!m_emptyPages.empty())
		{
			page = m_emptyPages.back();
			m_emptyPages.pop_back();
		}
		else
		{
			auto pageUP = std::make_unique<ParticlePage>();
			page = pageUP.get();
			m_pagesKeepAlive.push_back(std::move(pageUP));
		}
		
		page->emitterType = &emitterType;
		page->livingParticles = 0;
		m_pages.insert(it, page);
		
		return page;
	}
	
	void ParticleManager::Step(float dt, const Frustum& frustum, const glm::vec3& cameraForward)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_simulationDoneSignal.wait(lock, [&] { return m_state == State::SimulationDone; });
		
		for (int i = 0; i < m_missingUploadBuffers; i++)
			AddUploadBuffer();
		m_missingUploadBuffers = 0;
		
		m_instancesToDraw = 0;
		for (ParticleUploadBuffer& buffer : m_particleUploadBuffers)
		{
			m_instancesToDraw += buffer.instancesWritten;
		}
		
		if (m_instancesToDraw != 0)
		{
			if (m_instancesToDraw > m_deviceBufferCapacity)
			{
				m_deviceBufferCapacity = RoundToNextMultiple<uint32_t>(m_instancesToDraw, 16384);
				m_deviceBuffer = Buffer(BufferFlags::VertexBuffer | BufferFlags::CopyDst,
				                        m_deviceBufferCapacity * sizeof(ParticleInstance), nullptr);
			}
			
			uint64_t dstBufferOffset = 0;
			for (ParticleUploadBuffer& buffer : m_particleUploadBuffers)
			{
				if (buffer.instancesWritten == 0)
					continue;
				
				const uint64_t bytesToCopy = buffer.instancesWritten * sizeof(ParticleInstance);
				buffer.buffer.Flush(0, bytesToCopy);
				DC.CopyBuffer(buffer.buffer, m_deviceBuffer, 0, dstBufferOffset, bytesToCopy);
				dstBufferOffset += bytesToCopy;
				
				buffer.instancesWritten = 0;
				buffer.reuseDelay = MAX_CONCURRENT_FRAMES + 1;
			}
			
			m_deviceBuffer.UsageHint(BufferUsage::VertexBuffer);
		}
		
		for (int i = 0; i < 6; i++)
		{
			const Plane& p = frustum.GetPlane(i);
			m_frustumPlanes[i][0] = p.GetNormal().x;
			m_frustumPlanes[i][1] = p.GetNormal().y;
			m_frustumPlanes[i][2] = p.GetNormal().z;
			m_frustumPlanes[i][3] = -p.GetDistance();
		}
		
		m_cameraForward[0] = cameraForward.x;
		m_cameraForward[1] = cameraForward.y;
		m_cameraForward[2] = cameraForward.z;
		m_cameraForward[3] = 0.0f;
		
		m_currentTime += dt;
		
		//Updates the transform for old emitters
		for (size_t i = 0; i < m_btEmitters.size(); i++)
		{
			m_btEmitters[i].alive = m_mtEmitters[i].alive;
			m_btEmitters[i].transform = m_mtEmitters[i].transform;
			
			//if (!m_mtEmitters[i].alive)
			//	std::cout << "Emitter Dead" << std::endl;
		}
		
		//Removes dead emitters
		auto btEnd = std::remove_if(m_btEmitters.begin(), m_btEmitters.end(), [&] (const Emitter& e) { return !e.alive; });
		m_btEmitters.erase(btEnd, m_btEmitters.end());
		auto mtEnd = std::remove_if(m_mtEmitters.begin(), m_mtEmitters.end(), [&] (const Emitter& e) { return !e.alive; });
		m_mtEmitters.erase(mtEnd, m_mtEmitters.end());
		
		//Adds new emitters
		for (size_t i = m_btEmitters.size(); i < m_mtEmitters.size(); i++)
		{
			m_btEmitters.push_back(m_mtEmitters[i]);
		}
		
		m_state = State::Simulate;
		m_stepDoneSignal.notify_one();
	}
	
	ParticleEmitterInstance ParticleManager::AddEmitter(const ParticleEmitterType& type)
	{
		Emitter& emitter = m_mtEmitters.emplace_back();
		emitter.id = m_nextEmitterId++;
		emitter.type = &type;
		emitter.timeSinceEmit = 0;
		emitter.alive = true;
		emitter.hasSetTransform = false;
		emitter.gravity = glm::vec3(0, -5, 0);
		emitter.transform = glm::mat4(1.0f);
		emitter.prevTransform = glm::mat4(1.0f);
		emitter.UpdateEmissionDelay(1.0f);
		return ParticleEmitterInstance(emitter.id, this);
	}
	
	ParticleManager::Emitter& ParticleManager::GetEmitter(uint32_t id)
	{
		auto it = std::lower_bound(m_mtEmitters.begin(), m_mtEmitters.end(), id,
			[&] (const Emitter& a, uint32_t b) { return a.id < b; });
		if (it == m_mtEmitters.end() || it->id != id)
		{
			EG_PANIC("Invalid emitter id")
		}
		return *it;
	}
}