#include "camera.hpp"
#include <iostream>

namespace VE
{
	Camera::Camera(glm::vec3 position, glm::vec3 up)
	{
		m_Yaw = -90.0f;
		m_Pitch = 0.0f;
		m_Speed = 0.025f;
		m_Sensitivity = 0.1f;
		m_Zoom = 45.0f;

		m_Position = position;
		m_WorldUp = up;
		
		UpdateCameraVectors();
	}

	glm::mat4 Camera::GetViewMatrix()
	{
		return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
	}

	void Camera::ProcessCameraPosition(CameraMotion direction, float deltaTime)
	{
		float velocity = m_Speed * deltaTime;
		if (direction == CameraMotion::FORWARD)
			m_Position += m_Front * velocity;
		if (direction == CameraMotion::BACKWARD)
			m_Position -= m_Front * velocity;
		if (direction == CameraMotion::LEFT)
			m_Position -= m_Right * velocity;
		if (direction == CameraMotion::RIGHT)
			m_Position += m_Right * velocity;
		if (direction == CameraMotion::UP)
			m_Position += m_Up * velocity;
		if (direction == CameraMotion::DOWN)
			m_Position -= m_Up * velocity;

		// True fps cam
		m_Position.y = 0.0f;
	}

	void Camera::ProcessCameraDirection(glm::vec2 mouseOffset, bool constrainedPitch)
	{
		m_Yaw += mouseOffset.x * m_Sensitivity;
		m_Pitch += mouseOffset.y * m_Sensitivity;

		if (constrainedPitch)
		{
			if (m_Pitch > 89.0f)
				m_Pitch = 89.0f;
			if (m_Pitch < -89.0f)
				m_Pitch = -89.0f;
		}

		UpdateCameraVectors();
	}

	void Camera::UpdateCameraVectors()
	{
		// Calculate Front, Right and Up
		m_Front = glm::vec3(cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)),
			sin(glm::radians(m_Pitch)),
			sin(glm::radians(m_Yaw) * cos(glm::radians(m_Pitch))));
		m_Front = glm::normalize(m_Front);

		// OpenGL: Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement
		m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
		m_Up = glm::normalize(glm::cross(m_Right, m_Front));
	}
}