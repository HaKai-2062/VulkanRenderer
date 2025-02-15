#include <iostream>
#include <sstream>
#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "application.hpp"

namespace VE
{
	Application* Application::s_Application = nullptr;
	int Application::WIDTH = 1280;
	int Application::HEIGHT = 720;
	Camera Application::m_Camera;

	static glm::vec2 lastMousePos{ Application::WIDTH / 2.0f, Application::HEIGHT / 2.0f };
	bool firstMouse = true;
	
	static auto getDevice = []() -> VkDevice&
		{
			return Application::GetInstance()->m_VkInstance.GetDevice();
		};
	static auto getGraphicsQueue = []() ->VkQueue&
		{
			return Application::GetInstance()->m_VkInstance.GetGraphicsQueue();
		};
	static auto getSwapChain = []() ->VkSwapchainKHR&
		{
			return Application::GetInstance()->m_VkSwapChain.GetSwapChain();
		};

	Application::~Application()
	{
		// Calling it from Instance destructor causes crash so clearing early
		DestroySyncObjects();

		glfwDestroyWindow(m_Window);
		glfwTerminate();

		std::cout << "Application Destroyed" << std::endl;
	}

	void Application::Run()
	{
		s_Application = this;

		InitWindow();
		InitVulkan();
		InitStates();

		MainLoop();
	}

	void Application::InitWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Engine", nullptr, nullptr);
	}

	void Application::MainLoop()
	{
		while (!glfwWindowShouldClose(m_Window))
		{
			ProcessKeyboardInputs();

			DrawFrame();
			AddFPSToTitle();
			glfwPollEvents();
		}

		vkDeviceWaitIdle(getDevice());
	}

	void Application::InitVulkan()
	{
		m_VkInstance.CreateInstance();
		m_VkInstance.SetupDebugMessenger();
		m_VkInstance.CreateSurface();
		m_VkInstance.PickPhysicalDevice();
		m_VkInstance.CreateLogicalDevice();

		m_VkSwapChain.CreateSwapChain();
		m_VkSwapChain.CreateImageViews();

		m_VkPipeline.CreateRenderPass();
		m_VkDescriptors.CreateDescriptorSetLayout();
		m_VkPipeline.CreateGraphicsPipeline();

		m_VkBuffers.CreateCommandPool();
		m_VkBuffers.CreateColorResources();
		m_VkBuffers.CreateDepthResources();
		m_VkSwapChain.CreateFramebuffers();

		m_VkTexture.CreateTextureImage();
		m_VkTexture.CreateTextureImageView();
		m_VkTexture.CreateTextureSampler();

		m_VkModel.LoadModel();

		m_VkBuffers.CreateVertexBuffer();
		m_VkBuffers.CreateIndexBuffer();

		m_VkDescriptors.CreateUniformBuffers();
		m_VkDescriptors.CreateDescriptorPool();
		m_VkDescriptors.CreateDescriptorSets();

		m_VkBuffers.CreateCommandBuffer();
		
		CreateSyncObjects();
	}

	void Application::InitStates()
	{
		glfwSetCursorPosCallback(m_Window, Application::ProcessMouseInput);
		m_Camera.Init();
	}

	void Application::DrawFrame()
	{
		vkWaitForFences(getDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(getDevice(), getSwapChain(), UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("Failed to acquire swap chain image!");
		}

		glm::mat4 cameraViewMatrix = m_Camera.GetViewMatrix();

		m_VkDescriptors.UpdateUniformBuffer(m_CurrentFrame, cameraViewMatrix);

		vkResetFences(getDevice(), 1, &m_InFlightFences[m_CurrentFrame]);
		vkResetCommandBuffer(m_VkBuffers.GetCommandBuffers()[m_CurrentFrame], 0);
		m_VkBuffers.RecordCommandBuffer(m_VkBuffers.GetCommandBuffers()[m_CurrentFrame], imageIndex);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_VkBuffers.GetCommandBuffers()[m_CurrentFrame];

		VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(getGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { getSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		
		result = vkQueuePresentKHR(m_VkInstance.GetPresentQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized)
		{
			m_FramebufferResized = false;
			RecreateSwapChain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to present swap chain image!");
		}

		m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void Application::CreateSyncObjects()
	{
		m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(getDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(getDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(getDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create synchronization objects for a frame!");
			}
		}
	}

	void Application::RecreateSwapChain()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(m_Window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(m_Window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(getDevice());

		m_VkBuffers.DestroyDepthImage();
		m_VkBuffers.DestroyColorImage();
		m_VkSwapChain.~SwapChain();

		m_VkSwapChain.CreateSwapChain();
		m_VkSwapChain.CreateImageViews();
		m_VkBuffers.CreateColorResources();
		m_VkBuffers.CreateDepthResources();
		m_VkSwapChain.CreateFramebuffers();
	}


	void Application::DestroySyncObjects()
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(getDevice(), m_ImageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(getDevice(), m_RenderFinishedSemaphores[i], nullptr);
			vkDestroyFence(getDevice(), m_InFlightFences[i], nullptr);
		}
	}

	void Application::AddFPSToTitle()
	{
		static float lastTime = 0.0f;
		static uint32_t nFrames = 0;

		float currentTime = static_cast<float>(glfwGetTime());
		m_DeltaTime = currentTime - lastTime;
		nFrames++;

		if (m_DeltaTime >= 1.0)
		{
			uint32_t fps = static_cast<uint32_t>(nFrames / m_DeltaTime);

			float delay = static_cast<uint32_t>(100'000.0f / nFrames) / 100.0f;

			std::stringstream ss;
			ss << "Vulkan Engine" << "    [FPS: " << fps << "]     " << "[" << delay << " ms]";
			glfwSetWindowTitle(m_Window, ss.str().c_str());

			nFrames = 0;
			lastTime = currentTime;
		}
	}
	void Application::ProcessKeyboardInputs()
	{
		for (int key = 0; key < GLFW_KEY_LAST; key++)
		{
			m_CurrentKeyState[key] = glfwGetKey(m_Window, key) == GLFW_PRESS;
		}

		if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(m_Window, true);

		if (glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS)
			m_Camera.ProcessCameraPosition(CameraMotion::FORWARD, m_DeltaTime);
		if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS)
			m_Camera.ProcessCameraPosition(CameraMotion::BACKWARD, m_DeltaTime);
		if (glfwGetKey(m_Window, GLFW_KEY_A) == GLFW_PRESS)
			m_Camera.ProcessCameraPosition(CameraMotion::LEFT, m_DeltaTime);
		if (glfwGetKey(m_Window, GLFW_KEY_D) == GLFW_PRESS)
			m_Camera.ProcessCameraPosition(CameraMotion::RIGHT, m_DeltaTime);
		if (glfwGetKey(m_Window, GLFW_KEY_Q) == GLFW_PRESS)
			m_Camera.ProcessCameraPosition(CameraMotion::DOWN, m_DeltaTime);
		if (glfwGetKey(m_Window, GLFW_KEY_E) == GLFW_PRESS)
			m_Camera.ProcessCameraPosition(CameraMotion::UP, m_DeltaTime);

		// Trigger only on key release
		if (m_PreviousKeyState[GLFW_KEY_M] && !m_CurrentKeyState[GLFW_KEY_M])
		{
			if (m_MouseLocked)
				glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			else
				glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

			m_MouseLocked = !m_MouseLocked;
		}

		m_PreviousKeyState = m_CurrentKeyState;
	}

	void Application::ProcessMouseInput(GLFWwindow* window, double xPosIn, double yPosIn)
	{
		if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL)
			return;


		glm::vec2 mousePos = { static_cast<float>(xPosIn),  static_cast<float>(yPosIn) };

		if (firstMouse)
		{
			lastMousePos = mousePos;
			firstMouse = false;
		}
		
		glm::vec2 mouseOffset = { mousePos.x - lastMousePos.x, lastMousePos.y - mousePos.y };

		lastMousePos = mousePos;

		m_Camera.ProcessCameraDirection(mouseOffset);
	}

}