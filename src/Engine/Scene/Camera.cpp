#include "Scene/Camera.h"
#include "Core/Input.h"
#include "Core/Keycodes.h"

Camera::Camera(glm::vec3 position)
{
	m_Position = position;
}

void Camera::ProcessKeyEvents(float deltaTime)
{
	if (Input::IsKeyPressed(ENGINE_KEY_W))
		SetCameraPosition(CameraMotion::FORWARD, deltaTime);
	if (Input::IsKeyPressed(ENGINE_KEY_S))
		SetCameraPosition(CameraMotion::BACKWARD, deltaTime);
	if (Input::IsKeyPressed(ENGINE_KEY_A))
		SetCameraPosition(CameraMotion::LEFT, deltaTime);
	if (Input::IsKeyPressed(ENGINE_KEY_D))
		SetCameraPosition(CameraMotion::RIGHT, deltaTime);
	if (Input::IsKeyPressed(ENGINE_KEY_Q))
		SetCameraPosition(CameraMotion::DOWN, deltaTime);
	if (Input::IsKeyPressed(ENGINE_KEY_E))
		SetCameraPosition(CameraMotion::UP, deltaTime);
}

void Camera::ProcessMouseEvents(float deltaTime)
{
	if (Input::GetCursorState() == CursorState::Show)
		return;

	std::pair<double, double> deltaMouse = Input::GetDeltaMousePositions();
	glm::vec2 mouseOffset = { deltaMouse.first, deltaMouse.second };
	SetCameraDirection(mouseOffset);
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