#include <iostream>
#include <sstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <VkBootstrap.h>

#include "vk_types.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_engine.h"

#ifdef NDEBUG
static const bool bUseValidationLayers = false;
#else
static const bool bUseValidationLayers = true;
#endif

#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			std::cout <<"Detected Vulkan error: " << err << std::endl; \
			abort();                                                \
		}                                                           \
	} while (0)

void VulkanEngine::Init()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_Window = glfwCreateWindow(m_WindowExtent.width, m_WindowExtent.height, "Vulkan Engine", nullptr, nullptr);

	InitVulkan();
	InitSwapchain();
	InitCommands();
	InitSyncStructures();

	m_IsInitialized = true;
}
void VulkanEngine::Cleanup()
{
	if (m_IsInitialized)
	{
		vkDeviceWaitIdle(m_Device);

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroyCommandPool(m_Device, m_Frames[i].commandPool, nullptr);

			vkDestroyFence(m_Device, m_Frames[i].renderFence, nullptr);
			vkDestroySemaphore(m_Device, m_Frames[i].renderSemaphore, nullptr);
			vkDestroySemaphore(m_Device, m_Frames[i].swapchainSemaphore, nullptr);

			m_Frames[i].deletionQueue.flush();
		}

		// Flush global deletion queue
		m_DeletionQueue.flush();

		DestroySwapchain();
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

		vkDestroyDevice(m_Device, nullptr);
		vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger);

		glfwDestroyWindow(m_Window);
		vkDestroyInstance(m_Instance, nullptr);
		glfwTerminate();

		std::cout << "Application Destroyed" << std::endl;
	}
}

void VulkanEngine::DrawFrame()
{
	// Wait until the gpu has finished rendering the last frame. Timeout of 1e9 ns
	VK_CHECK(vkWaitForFences(m_Device, 1, &GetCurrentFrame().renderFence, true, 1000000000));
	GetCurrentFrame().deletionQueue.flush();
	VK_CHECK(vkResetFences(m_Device, 1, &GetCurrentFrame().renderFence));

	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(m_Device, m_Swapchain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex));

	VkCommandBuffer currentCMD = GetCurrentFrame().commandBuffer;
	VK_CHECK(vkResetCommandBuffer(currentCMD, 0));

	m_DrawExtent.width = m_DrawImage.imageExtent.width;
	m_DrawExtent.height = m_DrawImage.imageExtent.height;

	// Tell gpu that 1 submit per frame is happening so it optimizes for that
	VkCommandBufferBeginInfo currentCMDBeginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(currentCMD, &currentCMDBeginInfo));

	VkUtils::transitionImage(currentCMD, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	
	DrawBackground(currentCMD);

	VkUtils::transitionImage(currentCMD, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	VkUtils::transitionImage(currentCMD, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkUtils::copyImageToImage(currentCMD, m_DrawImage.image, m_SwapchainImages[swapchainImageIndex], m_DrawExtent, m_SwapchainExtent);
	VkUtils::transitionImage(currentCMD, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	
	VK_CHECK(vkEndCommandBuffer(currentCMD));

	//prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = VkInit::commandBufferSubmitInfo(currentCMD);
	VkSemaphoreSubmitInfo waitInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);
	
	VkSubmitInfo2 submit = VkInit::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, GetCurrentFrame().renderFence));

	//prepare present
	// this will put the image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that, 
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_GraphicsQueue, &presentInfo));

	m_FrameNumber++;
}

void VulkanEngine::MainLoop()
{
	while (!glfwWindowShouldClose(m_Window))
	{
		glfwPollEvents();

		AddFPSToTitle();
		DrawFrame();
	}
}

void VulkanEngine::InitVulkan()
{
	vkb::InstanceBuilder builder;

	auto returnedInstance = builder.set_app_name("Vulkan GameEngine")
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();

	vkb::Instance vkbInstance = returnedInstance.value();

	m_Instance = vkbInstance.instance;
	m_DebugMessenger = vkbInstance.debug_messenger;

	if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface!");
	}

	VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features.dynamicRendering = true;
	features.synchronization2 = true;
	
	VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	vkb::PhysicalDeviceSelector selector{ vkbInstance };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 3)
		.set_required_features_13(features) 
		.set_required_features_12(features12)
		.allow_any_gpu_device_type(false)
		.set_surface(m_Surface)
		.select()
		.value();

	vkb::DeviceBuilder deviceBuilder{ physicalDevice };
	vkb::Device vkbDevice = deviceBuilder.build().value();

	m_Device = vkbDevice.device;
	m_PhysicalDevice = physicalDevice.physical_device;

	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);
	
	if (bUseValidationLayers)
	{
		std::cout << "GPU: " << deviceProperties.deviceName << std::endl;
	}

	m_GraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_GraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = m_PhysicalDevice;
	allocatorInfo.device = m_Device;
	allocatorInfo.instance = m_Instance;
	// To use GPU pointers
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &m_Allocator);
	m_DeletionQueue.pushFunction([&]()
		{
			vmaDestroyAllocator(m_Allocator);
		});
}

void VulkanEngine::InitSwapchain()
{
	CreateSwapchain(m_WindowExtent.width, m_WindowExtent.height);

	VkExtent3D drawImageExtent =
	{
		m_WindowExtent.width,
		m_WindowExtent.height,
		1
	};

	m_DrawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	m_DrawImage.imageExtent = drawImageExtent;
	
	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo imageInfo = VkInit::imageCreateInfo(m_DrawImage.imageFormat, drawImageUsages, drawImageExtent);

	// Allocate GPU memory
	VmaAllocationCreateInfo imageAllocationInfo = {};
	imageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	imageAllocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(m_Allocator, &imageInfo, &imageAllocationInfo, &m_DrawImage.image, &m_DrawImage.allocation, nullptr);
	VkImageViewCreateInfo imageviewInfo = VkInit::imageviewCreateInfo(m_DrawImage.imageFormat, m_DrawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
	VK_CHECK(vkCreateImageView(m_Device, &imageviewInfo, nullptr, &m_DrawImage.imageView));

	m_DeletionQueue.pushFunction([&]()
		{
			vkDestroyImageView(m_Device, m_DrawImage.imageView, nullptr);
			vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);
		});
}

void VulkanEngine::InitCommands()
{
	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = nullptr;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = m_GraphicsQueueFamily;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_Frames[i].commandPool));
		VkCommandBufferAllocateInfo cmdAllocInfo = VkInit::commandBufferAllocateInfo(m_Frames[i].commandPool, 1);
		VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_Frames[i].commandBuffer));
	}
}

void VulkanEngine::InitSyncStructures()
{
	//create syncronization structures
	//one fence to control when the gpu has finished rendering the frame,
	//and 2 semaphores to syncronize rendering with swapchain
	//we want the fence to start signalled so we can wait on it on the first frame
	
	VkFenceCreateInfo fenceCreateInfo = VkInit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VkSemaphoreCreateInfo semaphoreCreateInfo = VkInit::semaphoreCreateInfo();

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_Frames[i].renderFence));

		VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].swapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].renderSemaphore));
	}
}

void VulkanEngine::CreateSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ m_PhysicalDevice, m_Device, m_Surface };
	m_SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		//.use_default_format_selection()
		.set_desired_format(VkSurfaceFormatKHR{ .format = m_SwapchainFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build()
		.value();

	m_SwapchainExtent = vkbSwapchain.extent;
	m_Swapchain = vkbSwapchain.swapchain;
	m_SwapchainImages = vkbSwapchain.get_images().value();
	m_SwapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::DestroySwapchain()
{
	vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);

	for (int i = 0; i < m_SwapchainImageViews.size(); i++)
	{
		vkDestroyImageView(m_Device, m_SwapchainImageViews[i], nullptr);
	}
}

void VulkanEngine::DrawBackground(VkCommandBuffer& currentCMD)
{
	VkClearColorValue clearValue;
	float flash = std::abs(std::sin(m_FrameNumber / 120.f));
	float flash2 = std::abs(std::sin(m_FrameNumber / 120.f + 20.0));
	float flash3 = std::abs(std::sin(m_FrameNumber / 120.f + 40.0));
	clearValue = { { flash, flash2, flash3, 1.0f } };
	VkImageSubresourceRange clearRange = VkInit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdClearColorImage(currentCMD, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
}

void VulkanEngine::AddFPSToTitle()
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