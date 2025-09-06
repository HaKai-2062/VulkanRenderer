#include "Core/Input.h"

std::pair<double, double> Input::m_MousePos = std::make_pair(0.0, 0.0);
std::pair<double, double> Input::m_LastMousePos = std::make_pair(0.0, 0.0);
std::array<bool, ENGINE_KEY_LAST> Input::m_PreviousKeyState{};
std::array<bool, ENGINE_KEY_LAST> Input::m_CurrentKeyState{};

void Input::SetCursorState(CursorState cursorState)
{
	if (cursorState == CursorState::Disabled)
		BackEndWindow::SetCursorDisable();
	else if (cursorState == CursorState::Show)
		BackEndWindow::SetCursorShow();
	else
		BackEndWindow::SetCursorHide();
}

CursorState Input::GetCursorState()
{
	if (BackEndWindow::IsCursorDisabled())
		return CursorState::Disabled;
	else if (BackEndWindow::IsCursorShown())
		return CursorState::Show;

	return CursorState::Hidden;
}

bool Input::IsKeyPressed(uint32_t key)
{
	return m_CurrentKeyState[key];
}

bool Input::IsKeyReleased(uint32_t key)
{
	return m_PreviousKeyState[key] && !m_CurrentKeyState[key];
}

void Input::BeginFrame()
{
	for (int key = 0; key < ENGINE_KEY_LAST; key++)
	{
		m_CurrentKeyState[key] = BackEndWindow::IsKeyPressed(key);
	}

	if (BackEndWindow::IsKeyPressed(ENGINE_KEY_ESCAPE))
	{
		BackEndWindow::CloseWindow();
	}

	// Mouse events
	m_MousePos = BackEndWindow::GetCursorPosition();

	if (IsKeyReleased(ENGINE_KEY_M))
	{
		if (GetCursorState() == CursorState::Disabled)
			SetCursorState(CursorState::Show);
		else
			SetCursorState(CursorState::Disabled);
	}
}

void Input::EndFrame()
{
	m_PreviousKeyState = m_CurrentKeyState;
	m_LastMousePos = m_MousePos;
}
