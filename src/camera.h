#pragma once

#include "vk_types.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <GLFW/glfw3.h>
#include <array>

class Camera
{
public:
	Camera(glm::vec3 position = glm::vec3(30.f, -00.f, -085.f));
	void ProcessKeyEvents(GLFWwindow* window, float deltaTime);
	void ProcessMouseEvents(GLFWwindow* window, double xPosIn, double yPosIn);
	void SetCameraPosition(CameraMotion direction, float deltaTime);
	void SetCameraDirection(glm::vec2 mouseoffset, bool constrainedPitch = true);
	glm::mat4 GetViewMatrix();

public:
	float Yaw = 0.0f;
	float Pitch = 0.0f;

private:
	glm::vec3 m_Position{ 0.0f }; 
	glm::vec3 m_Front{ 0.0f };
	glm::vec3 m_Up{ 0.0f, 1.0f, 0.0f };
	glm::vec3 m_Right{ 0.0f };
	glm::quat m_Orientation{ 1.0f, 0.0f, 0.0f, 0.0f };

	float m_Speed = 2.5f;
	float m_Sensitivity = 0.05f;
	float m_Zoom = 45.0f;

	bool m_MouseLocked = false;
	std::array<bool, GLFW_KEY_LAST> m_PreviousKeyState{};
	std::array<bool, GLFW_KEY_LAST> m_CurrentKeyState{};
	glm::vec2 m_LastMousePos{ 0.0f, 0.0f };
	bool m_FirstMouse = true;
};