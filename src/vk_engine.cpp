#include <iostream>
#include <GLFW/glfw3.h>

#include "vk_engine.h"
#include "vk_types.h"
#include "vk_initializers.h"

void VulkanEngine::Init()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_Window = glfwCreateWindow(m_WindowExtent.width, m_WindowExtent.height, "Vulkan Engine", nullptr, nullptr);

	m_IsInitialized = true;
}
void VulkanEngine::Cleanup()
{
	if (m_IsInitialized)
	{
		glfwDestroyWindow(m_Window);
		glfwTerminate();

		std::cout << "Application Destroyed" << std::endl;
	}
}

void VulkanEngine::DrawFrame()
{

}

void VulkanEngine::MainLoop()
{
	while (!glfwWindowShouldClose(m_Window))
	{
		glfwPollEvents();

		DrawFrame();
	}
}
