#pragma once

#include "../Utils.hpp"

namespace eg
{
class EG_API PerspectiveProjection
{
public:
	PerspectiveProjection() : m_fieldOfViewRad(HALF_PI * 0.9f), m_zNear(0.1f), m_zFar(1000.0f), m_aspectRatio(1.0f) {}

	void SetFieldOfViewRad(float fieldOfViewRad)
	{
		m_fieldOfViewRad = fieldOfViewRad;
		Update();
	}

	void SetFieldOfViewDeg(float fieldOfViewDeg)
	{
		m_fieldOfViewRad = glm::radians(fieldOfViewDeg);
		Update();
	}

	void SetZNear(float zNear)
	{
		m_zNear = zNear;
		Update();
	}

	void SetZFar(float zFar)
	{
		m_zFar = zFar;
		Update();
	}

	void SetAspectRatio(float aspectRatio)
	{
		m_aspectRatio = aspectRatio;
		Update();
	}

	void SetResolution(float width, float height)
	{
		m_aspectRatio = width / height;
		Update();
	}

	float FieldOfViewRad() const { return m_fieldOfViewRad; }

	float FieldOfViewDeg() const { return glm::degrees(m_fieldOfViewRad); }

	float ZNear() const { return m_zNear; }

	float ZFar() const { return m_zFar; }

	float AspectRatio() const { return m_aspectRatio; }

	const glm::mat4& Matrix() const { return m_matrix; }

	const glm::mat4& InverseMatrix() const { return m_inverseMatrix; }

private:
	void Update();

	glm::mat4 m_matrix;
	glm::mat4 m_inverseMatrix;

	float m_fieldOfViewRad;
	float m_zNear;
	float m_zFar;
	float m_aspectRatio;
};
} // namespace eg
