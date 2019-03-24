#include "TranslationGizmo.hpp"
#include "Plane.hpp"
#include "../Shaders/Build/Gizmo.vs.h"
#include "../Shaders/Build/Gizmo.fs.h"

namespace eg
{
	const glm::vec3 ARROW_VERTICES[] = 
	{
		glm::vec3(0.000000, -0.000000, -0.067516),
		glm::vec3(0.651097, 0.000000, -0.067516),
		glm::vec3(0.000000, -0.047741, -0.047741),
		glm::vec3(0.651097, -0.047741, -0.047741),
		glm::vec3(0.000000, -0.067516, 0.000000),
		glm::vec3(0.651097, -0.067516, 0.000000),
		glm::vec3(0.000000, -0.047741, 0.047741),
		glm::vec3(0.651097, -0.047741, 0.047741),
		glm::vec3(0.000000, -0.000000, 0.067516),
		glm::vec3(0.651097, 0.000000, 0.067516),
		glm::vec3(0.000000, 0.047741, 0.047741),
		glm::vec3(0.651097, 0.047741, 0.047741),
		glm::vec3(0.000000, 0.067516, -0.000000),
		glm::vec3(0.651097, 0.067516, -0.000000),
		glm::vec3(0.000000, 0.047741, -0.047741),
		glm::vec3(0.651097, 0.047741, -0.047741),
		glm::vec3(0.651097, -0.101067, -0.101067),
		glm::vec3(0.651097, 0.000000, -0.142930),
		glm::vec3(0.651097, -0.142930, 0.000000),
		glm::vec3(0.651097, -0.101067, 0.101067),
		glm::vec3(0.651097, 0.000000, 0.142930),
		glm::vec3(0.651097, 0.101067, 0.101067),
		glm::vec3(0.651097, 0.142930, -0.000000),
		glm::vec3(0.651097, 0.101067, -0.101067),
		glm::vec3(0.999446, 0.000000, -0.000000)
	};
	
	uint16_t ARROW_INDICES[] = 
	{
		2, 3, 1,
		4, 5, 3,
		6, 7, 5,
		8, 9, 7,
		10, 11, 9,
		12, 13, 11,
		8, 19, 20,
		14, 15, 13,
		16, 1, 15,
		7, 11, 15,
		24, 23, 25,
		14, 22, 23,
		10, 20, 21,
		14, 24, 16,
		4, 19, 6,
		12, 21, 22,
		16, 18, 2,
		2, 17, 4,
		22, 21, 25,
		20, 19, 25,
		17, 18, 25,
		18, 24, 25,
		23, 22, 25,
		21, 20, 25,
		19, 17, 25,
		2, 4, 3,
		4, 6, 5,
		6, 8, 7,
		8, 10, 9,
		10, 12, 11,
		12, 14, 13,
		8, 6, 19,
		14, 16, 15,
		16, 2, 1,
		15, 1, 3,
		3, 5, 7,
		7, 9, 11,
		11, 13, 15,
		15, 3, 7,
		14, 12, 22,
		10, 8, 20,
		14, 23, 24,
		4, 17, 19,
		12, 10, 21,
		16, 24, 18,
		2, 18, 17
	};
	
	const uint32_t NUM_ARROW_INDICES = ArrayLen(ARROW_INDICES);
	
	static Buffer s_arrowVB;
	static Buffer s_arrowIB;
	static Pipeline s_pipeline;
	
	const glm::vec3 ARROW_OFFSET = glm::vec3(0.2f, 0.0f, 0.0f);
	const glm::vec3 ARROW_SCALE = glm::vec3(0.8f, 0.6f, 0.6f);
	
	const float AXIS_LIGHTNESS = 0.25f;
	const glm::vec3 AXIS_COLORS[] = 
	{
		glm::vec3(1.0f, AXIS_LIGHTNESS, AXIS_LIGHTNESS),
		glm::vec3(AXIS_LIGHTNESS, 1.0f, AXIS_LIGHTNESS),
		glm::vec3(AXIS_LIGHTNESS, AXIS_LIGHTNESS, 1.0f),
	};
	
	const glm::vec3 CURRENT_AXIS_COLOR(1.0f, 1.0f, 0.5f);
	
	void TranslationGizmo::InitStatic()
	{
		for (uint16_t& i : ARROW_INDICES)
			i--;
		s_arrowVB = eg::Buffer(BufferFlags::VertexBuffer, sizeof(ARROW_VERTICES), ARROW_VERTICES);
		s_arrowIB = eg::Buffer(BufferFlags::IndexBuffer, sizeof(ARROW_INDICES), ARROW_INDICES);
		
		s_arrowVB.UsageHint(BufferUsage::VertexBuffer);
		s_arrowIB.UsageHint(BufferUsage::IndexBuffer);
		
		ShaderModule vs(ShaderStage::Vertex, { reinterpret_cast<const char*>(Gizmo_vs_glsl), sizeof(Gizmo_vs_glsl) });
		ShaderModule fs(ShaderStage::Fragment, { reinterpret_cast<const char*>(Gizmo_fs_glsl), sizeof(Gizmo_fs_glsl) });
		
		GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = vs.Handle();
		pipelineCI.fragmentShader = fs.Handle();
		pipelineCI.vertexBindings[0] = { sizeof(float) * 3, InputRate::Vertex };
		pipelineCI.vertexAttributes[0] = { 0, DataType::Float32, 3, 0 };
		s_pipeline = Pipeline::Create(pipelineCI);
	}
	
	void TranslationGizmo::DestroyStatic()
	{
		s_arrowVB.Destroy();
		s_arrowIB.Destroy();
		s_pipeline.Destroy();
	}
	
	static bool RayIntersectsArrow(const glm::mat4& worldMatrix, const Ray& ray)
	{
		glm::vec3 arrowVerticesWS[ArrayLen(ARROW_VERTICES)];
		for (size_t i = 0; i < ArrayLen(ARROW_VERTICES); i++)
		{
			arrowVerticesWS[i] = glm::vec3(worldMatrix * glm::vec4(ARROW_VERTICES[i], 1.0f));
		}
		
		for (uint32_t i = 0; i < NUM_ARROW_INDICES; i += 3)
		{
			const glm::vec3& v0 = arrowVerticesWS[ARROW_INDICES[i + 0]];
			const glm::vec3& v1 = arrowVerticesWS[ARROW_INDICES[i + 1]];
			const glm::vec3& v2 = arrowVerticesWS[ARROW_INDICES[i + 2]];
			
			Plane plane(v0, v1, v2);
			
			float intersectDist;
			if (ray.Intersects(plane, intersectDist) && intersectDist > 0)
			{
				glm::vec3 intersectPos = ray.GetPoint(intersectDist);
				if (TriangleContainsPoint(v0, v1, v2, intersectPos))
					return true;
			}
		}
		
		return false;
	}
	
	TranslationGizmo::TranslationGizmo()
	{
		for (int i = 0; i < 3; i++)
		{
			m_axisDrawOrder[i] = i;
		}
	}
	
	void TranslationGizmo::Update(glm::vec3& position, const glm::vec3& cameraPos, const glm::mat4& viewProjMatrix, const Ray& viewRay)
	{
		m_renderScale = glm::distance(cameraPos, position) * m_sizeScale;
		
		//Calculates the depth of the end of each arrow
		float arrowDepths[3];
		for (int i = 0; i < 3; i++)
		{
			glm::vec3 endPos = position;
			endPos[i] += ARROW_SCALE.x * m_renderScale;
			
			glm::vec4 endPosPPS = viewProjMatrix * glm::vec4(endPos, 1.0f);
			arrowDepths[i] = endPosPPS.z / endPosPPS.w;
		}
		
		std::sort(std::begin(m_axisDrawOrder), std::end(m_axisDrawOrder), [&] (int a, int b)
		{
			return arrowDepths[a] > arrowDepths[b];
		});
		
		auto BeginDragAxis = [&] (int index)
		{
			m_currentAxis = index;
			
			glm::vec3 dragDirection;
			dragDirection[index] = 1;
			m_axisDragRay = { position, dragDirection };
			
			m_initialDragDist = m_axisDragRay.GetClosestPoint(viewRay);
			m_keyboardSelectingAxis = false;
		};
		
		//Handles keyboard input
		if (!m_keyboardSelectingAxis)
		{
			if (IsButtonDown(Button::G) && !WasButtonDown(Button::G))
				m_keyboardSelectingAxis = true;
		}
		else
		{
			if (IsButtonDown(Button::X) && !WasButtonDown(Button::X))
				BeginDragAxis(0);
			if (IsButtonDown(Button::Y) && !WasButtonDown(Button::Y))
				BeginDragAxis(1);
			if (IsButtonDown(Button::Z) && !WasButtonDown(Button::Z))
				BeginDragAxis(2);
			if (IsButtonDown(Button::Escape) && !WasButtonDown(Button::Escape))
				m_keyboardSelectingAxis = false;
		}
		
		bool select = m_currentAxis == -1 && IsButtonDown(Button::MouseLeft) && !WasButtonDown(Button::MouseLeft);
		
		//Resets the current axis if the mouse button was released.
		if (WasButtonDown(Button::MouseLeft) && !IsButtonDown(Button::MouseLeft))
		{
			m_currentAxis = -1;
		}
		
		//Drags the gizmo
		if (m_currentAxis != -1)
		{
			float dragDist = m_axisDragRay.GetClosestPoint(viewRay);
			if (!std::isnan(dragDist))
			{
				position = m_axisDragRay.GetPoint(dragDist - m_initialDragDist);
			}
		}
		
		m_hoveredAxis = -1;
		for (int axis : m_axisDrawOrder)
		{
			//Initializes the rotation & m_renderScale matrix
			glm::mat4 rotation(0.0f);
			for (int i = 0; i < 3; i++)
				rotation[i][(axis + i) % 3] = 1;
			rotation[3][3] = 1;
			
			glm::mat4 worldMatrix = glm::translate(glm::mat4(), position) *
			                        rotation *
			                        glm::translate(glm::mat4(), ARROW_OFFSET * m_renderScale) *
			                        glm::scale(glm::mat4(), ARROW_SCALE * m_renderScale);
			
			if (RayIntersectsArrow(worldMatrix, viewRay))
			{
				m_hoveredAxis = axis;
				if (select)
					m_currentAxis = axis;
			}
		}
		
		if (select && m_currentAxis != -1)
		{
			BeginDragAxis(m_currentAxis);
			select = false;
		}
		
		m_lastPosition = position;
	}
	
	void TranslationGizmo::Draw(const glm::mat4& viewProjMatrix) const
	{
		DC.BindPipeline(s_pipeline);
		DC.BindVertexBuffer(0, s_arrowVB, 0);
		DC.BindIndexBuffer(IndexType::UInt16, s_arrowIB, 0);
		
		for (int axis : m_axisDrawOrder)
		{
			//Initializes the rotation & m_renderScale matrix
			glm::mat4 rotation(0.0f);
			for (int i = 0; i < 3; i++)
				rotation[i][(axis + i) % 3] = 1;
			rotation[3][3] = 1;
			
			glm::mat4 worldMatrix = glm::translate(glm::mat4(), m_lastPosition) *
			                        rotation *
			                        glm::translate(glm::mat4(), ARROW_OFFSET * m_renderScale) *
			                        glm::scale(glm::mat4(), ARROW_SCALE * m_renderScale);
			
			glm::vec3 color = AXIS_COLORS[axis];
			
			if (m_currentAxis == axis)
			{
				color = CURRENT_AXIS_COLOR;
			}
			else if (m_currentAxis == -1 && m_hoveredAxis == axis)
			{
				color *= 2.0f;
			}
			
			struct PC
			{
				glm::mat4 transform;
				glm::vec4 color;
			} pc;
			pc.transform = viewProjMatrix * worldMatrix;
			pc.color = glm::vec4(color, 1.0f);
			
			DC.PushConstants(0, pc);
			
			DC.DrawIndexed(0, NUM_ARROW_INDICES, 0, 0, 1);
		}
	}
}
