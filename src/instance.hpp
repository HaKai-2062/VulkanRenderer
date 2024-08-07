#pragma once

#include <vector>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "swapchain.hpp"

namespace VE
{
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool IsComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	class Instance
	{
	public:
		Instance() = default;
		~Instance();

		VkDevice& GetDevice() { return m_Device; }
		VkPhysicalDevice& GetPhysicalDevice() { return m_PhysicalDevice; }
		VkSurfaceKHR& GetSurface() { return m_Surface; }
		VkQueue& GetGraphicsQueue() { return m_GraphicsQueue; }
		VkQueue& GetPresentQueue() { return m_PresentQueue; }

		void CreateInstance();
		void SetupDebugMessenger();
		void CreateSurface();
		void PickPhysicalDevice();
		void CreateLogicalDevice();
		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
		static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);

	private:
		bool IsDeviceSuitable(VkPhysicalDevice device);
		bool CheckValidationLayerSupport();
		bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
		std::vector<const char*> GetRequiredExtensions();
		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

	private:
		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_Device;
		VkQueue m_GraphicsQueue;
		VkSurfaceKHR m_Surface;
		VkQueue m_PresentQueue;
	};

}

