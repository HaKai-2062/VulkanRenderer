#include "camera.hpp"

namespace VE
{
	void Camera::Init(glm::vec3 position, glm::vec3 up)
	{
		m_Yaw = -90.0f;
		m_Pitch = 0.0f;
		m_Speed = 0.025f;
		m_Sensitivity = 0.1f;
		m_Zoom = 45.0f;

		m_Position = position;
		m_Up = up;

		m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
		m_Orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	}

	glm::mat4 Camera::GetViewMatrix()
	{
		m_Front = m_Orientation * glm::vec3(0.0f, 0.0f, -1.0f);
		m_Up = m_Orientation * glm::vec3(0.0f, 1.0f, 0.0f);
		return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
	}

	void Camera::ProcessCameraPosition(CameraMotion direction, float deltaTime)
	{
		m_Front = m_Orientation * glm::vec3(0.0f, 0.0f, -1.0f);
		m_Right = m_Orientation * glm::vec3(1.0f, 0.0f, 0.0f);
		//m_Up = m_Orientation * glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 localUp = glm::vec3(0.0f, 0.0f, 1.0f);

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
			m_Position += localUp * velocity;
		if (direction == CameraMotion::DOWN)
			m_Position -= localUp * velocity;

		// True fps cam
		//m_Position.y = 0.0f;
	}

	void Camera::ProcessCameraDirection(glm::vec2 mouseOffset, bool constrainedPitch)
	{
		m_Yaw -= mouseOffset.x * m_Sensitivity;
		m_Pitch += mouseOffset.y * m_Sensitivity;

		if (constrainedPitch)
		{
			if (m_Pitch > 89.0f)
				m_Pitch = 89.0f;
			if (m_Pitch < -89.0f)
				m_Pitch = -89.0f;
		}

		glm::quat yawQuat = glm::angleAxis(glm::radians(m_Yaw), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::quat pitchQuat = glm::angleAxis(glm::radians(m_Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
		m_Orientation = pitchQuat * yawQuat;
		m_Orientation = glm::normalize(m_Orientation);
	}
}