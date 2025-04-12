#include "camera.h"

#include <iostream>

Camera::Camera(glm::vec3 position)
{
	m_Position = position;
}

void Camera::ProcessKeyEvents(GLFWwindow* window, float deltaTime)
{
	for (int key = 0; key < GLFW_KEY_LAST; key++)
	{
		m_CurrentKeyState[key] = glfwGetKey(window, key) == GLFW_PRESS;
	}

	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		SetCameraPosition(CameraMotion::FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		SetCameraPosition(CameraMotion::BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		SetCameraPosition(CameraMotion::LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		SetCameraPosition(CameraMotion::RIGHT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		SetCameraPosition(CameraMotion::DOWN, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		SetCameraPosition(CameraMotion::UP, deltaTime);

	// Trigger only on key release
	if (m_PreviousKeyState[GLFW_KEY_M] && !m_CurrentKeyState[GLFW_KEY_M])
	{
		if (m_MouseLocked)
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		else
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		m_MouseLocked = !m_MouseLocked;
	}

	m_PreviousKeyState = m_CurrentKeyState;
}

glm::mat4 Camera::GetViewMatrix()
{
	m_Front = m_Orientation * glm::vec3(0.0f, 0.0f, -1.0f);
	m_Up = m_Orientation * glm::vec3(0.0f, 1.0f, 0.0f);
	return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
}

void Camera::SetCameraPosition(CameraMotion direction, float deltaTime)
{
	m_Front = m_Orientation * glm::vec3(0.0f, 0.0f, -1.0f);
	m_Right = m_Orientation * glm::vec3(1.0f, 0.0f, 0.0f);
	//m_Up = m_Orientation * glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 localUp = glm::vec3(0.0f, 1.0f, 0.0f);

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

void Camera::SetCameraDirection(glm::vec2 mouseOffset, bool constrainedPitch)
{
	Yaw -= mouseOffset.x * m_Sensitivity;
	Pitch += mouseOffset.y * m_Sensitivity;

	if (constrainedPitch)
	{
		if (Pitch > 89.0f)
			Pitch = 89.0f;
		if (Pitch < -89.0f)
			Pitch = -89.0f;
	}

	glm::quat yawQuat = glm::angleAxis(glm::radians(Yaw), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat pitchQuat = glm::angleAxis(glm::radians(Pitch), glm::vec3(1.0f, 0.0f, 0.0f));
	m_Orientation = yawQuat * pitchQuat;
	m_Orientation = glm::normalize(m_Orientation);
}