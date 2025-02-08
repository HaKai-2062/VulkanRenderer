#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "enums.hpp"

namespace VE
{
	class Camera
	{
	public:
		Camera() = default;
		~Camera() = default;

		void Init(glm::vec3 position = glm::vec3(2.0f, 1.0f, 2.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f));
		void ProcessCameraPosition(CameraMotion direction, float deltaTime);
		void ProcessCameraDirection(glm::vec2 mouseoffset, bool constrainedPitch = true);
		glm::mat4 GetViewMatrix();
	private:
		void UpdateCameraVectors();

	public:
		float m_Yaw;
		float m_Pitch;

	private:
		glm::vec3 m_Position;
		glm::vec3 m_Front;
		glm::vec3 m_Up;
		glm::vec3 m_Right;
		glm::vec3 m_WorldUp;


		float m_Speed;
		float m_Sensitivity;
		float m_Zoom;
	};
}