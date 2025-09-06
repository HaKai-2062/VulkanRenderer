#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Core/BackEndWindow.h"

BackEndWindow::BackEndWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_Window = glfwCreateWindow(m_WindowExtent.first, m_WindowExtent.second, "Engine", nullptr, nullptr);
}

BackEndWindow::~BackEndWindow()
{
	glfwTerminate();
}

void BackEndWindow::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
	if (glfwCreateWindowSurface(instance, GetWindow(), nullptr, surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface!");
	}
}

std::pair<uint32_t, uint32_t> BackEndWindow::GetWindowSize()
{
	int x, y;
	glfwGetWindowSize(GetWindow(), &x, &y);
	Get().m_WindowExtent = std::make_pair(x, y);
	return Get().m_WindowExtent;
}

void BackEndWindow::SetWindowTitle(const char* title) { glfwSetWindowTitle(GetWindow(), title); }
void BackEndWindow::BeginFrame() { glfwPollEvents(); }
void BackEndWindow::CloseWindow() { glfwSetWindowShouldClose(GetWindow(), GLFW_TRUE); }
void BackEndWindow::DestroyWindow() { glfwDestroyWindow(GetWindow()); }
bool BackEndWindow::ShouldWindowClose() { return glfwWindowShouldClose(GetWindow()); }

void BackEndWindow::SetCursorShow() { glfwSetInputMode(GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL); }
void BackEndWindow::SetCursorHide() { glfwSetInputMode(GetWindow(), GLFW_CURSOR, GLFW_CURSOR_HIDDEN); }
void BackEndWindow::SetCursorDisable() { glfwSetInputMode(GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED); }
std::pair<double, double> BackEndWindow::GetCursorPosition()
{ 
	double x, y;
	glfwGetCursorPos(GetWindow(), &x, &y);
	return std::make_pair(x, y);
}

bool BackEndWindow::IsCursorShown() { return glfwGetInputMode(GetWindow(), GLFW_CURSOR) == GLFW_CURSOR_NORMAL; }
bool BackEndWindow::IsCursorHidden() { return glfwGetInputMode(GetWindow(), GLFW_CURSOR) == GLFW_CURSOR_HIDDEN; }
bool BackEndWindow::IsCursorDisabled() { return glfwGetInputMode(GetWindow(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED; }
bool BackEndWindow::IsKeyPressed(uint32_t key) { return glfwGetKey(GetWindow(), static_cast<int>(key)) == GLFW_PRESS; }
bool BackEndWindow::IsWindowMinimized() { return glfwGetWindowAttrib(GetWindow(), GLFW_ICONIFIED); }

double BackEndWindow::GetTime() { return glfwGetTime(); }