#pragma once

#include "vk_types.h"

class VulkanEngine
{
public:

	bool m_IsInitialized{ false };
	int m_FrameNumber{ 0 };

	VkExtent2D m_WindowExtent{ 1700 , 900 };

	struct GLFWwindow* m_Window{ nullptr };

	void Init();
	void Cleanup();
	void DrawFrame();
	void MainLoop();
};