#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

enum class CameraMotion : uint8_t
{
	LEFT = 0,
	RIGHT,
	FORWARD,
	BACKWARD,
	UP,
	DOWN
};

class Camera
{
public:
	Camera(glm::vec3 position = glm::vec3(10.0f, 5.0f, 25.f));
	const glm::vec3 GetCameraPosition() { return m_Position; }
	const glm::vec3 GetCameraOrientation() { return m_Front; }
	void ProcessKeyEvents(float deltaTime);
	void ProcessMouseEvents(float deltaTime);
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

	float m_Speed = 5.0f;
	float m_Sensitivity = 0.05f;
	float m_Zoom = 45.0f;
};