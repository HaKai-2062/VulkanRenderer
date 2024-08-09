#pragma once

#include "instance.hpp"
#include "swapchain.hpp"
#include "pipeline.hpp"
#include "buffers.hpp"
#include "descriptors.hpp"
#include "texture.hpp"
#include "model.hpp"

struct GLFWwindow;

namespace VE
{
	const int MAX_FRAMES_IN_FLIGHT = 2;

	class Application
	{
	public:
		~Application();

		void Run();
		void DrawFrame();
		void CreateSyncObjects();
		void DestroySyncObjects();

		static Application* GetInstance() { return s_Application; }
		static GLFWwindow* GetWindowInstance() { return GetInstance()->m_Window; }
		const uint32_t GetCurrentFrame() { return m_CurrentFrame; }

	public:
		int WIDTH = 1280;
		int HEIGHT = 720;
		// Destructors are called in the order they are arranged
		Instance m_VkInstance;
		Pipeline m_VkPipeline;
		Buffers m_VkBuffers;
		Texture m_VkTexture;
		Descriptors m_VkDescriptors;
		SwapChain m_VkSwapChain;
		Model m_VkModel;

	private:
		void InitWindow();
		void InitVulkan();
		void MainLoop();
		void RecreateSwapChain();

		//void CleanUpSwapChain();

	private:
		static Application* s_Application;
		GLFWwindow* m_Window;

		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence> m_InFlightFences;
		uint32_t m_CurrentFrame = 0;
		bool m_FramebufferResized = false;

		//CommandBuffer m_CommandBuffer;
		//Model m_Model;
		//Texture m_Texture;
	};
}
