#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace VE
{
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};


	class SwapChain
	{
	public:
		SwapChain() = default;
		~SwapChain();

		void CreateSwapChain();
		void CreateImageViews();
		void CreateFramebuffers();
		static VkImageView CreateImageView(VkImage image, VkFormat format);

		VkSwapchainKHR& GetSwapChain() { return m_SwapChain; }
		VkExtent2D& GetSwapChainExtent() { return m_SwapChainExtent; }
		VkFormat& GetSwapChainFormat() { return m_SwapChainImageFormat; }
		const std::vector<VkImageView>& GetSwapChainImageViews() { return m_SwapChainImageViews; }
		const std::vector<VkFramebuffer>& GetSwapChainFramebuffers() { return m_SwapChainFramebuffers; }

	private:
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
		
	private:
		VkSwapchainKHR m_SwapChain;
		VkExtent2D m_SwapChainExtent;
		VkFormat m_SwapChainImageFormat;
		std::vector<VkImage> m_SwapChainImages;
		std::vector<VkImageView> m_SwapChainImageViews;
		std::vector<VkFramebuffer> m_SwapChainFramebuffers;
	};
}