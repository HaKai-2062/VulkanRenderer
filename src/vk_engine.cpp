#include <sstream>
#include <array>
#include <chrono>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include <VkBootstrap.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "vk_types.h"
#include "vk_initializers.h"
#include "vk_images.h"
#include "vk_engine.h"
#include "vk_pipelines.h"

#ifdef NDEBUG
static const bool bUseValidationLayers = false;
#else
static const bool bUseValidationLayers = true;
#endif

void VulkanEngine::Init()
{
	fmt::print(fmt::fg(fmt::color::green), "Application Created\n");

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_Window = glfwCreateWindow(m_WindowExtent.width, m_WindowExtent.height, "Vulkan Engine", nullptr, nullptr);

	InitVulkan();
	InitSwapchain();
	InitCommands();
	InitSyncStructures();
	InitDescriptors();
	InitPipelines();
	InitImGui();
	InitDefaultData();

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

		for (auto& mesh : m_TestMeshes)
		{
			DestroyBuffer(mesh->meshBuffers.indexBuffer);
			DestroyBuffer(mesh->meshBuffers.vertexBuffer);
		}

		// Flush global deletion queue
		m_MainDeletionQueue.flush();

		DestroySwapchain();
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

		vkDestroyDevice(m_Device, nullptr);
		vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger);

		glfwDestroyWindow(m_Window);
		vkDestroyInstance(m_Instance, nullptr);
		glfwTerminate();

		fmt::print(fmt::fg(fmt::color::yellow) | fmt::bg(fmt::color::black), "Application Destroyed\n");
	}
}

void VulkanEngine::DrawFrame()
{
	// Wait until the gpu has finished rendering the last frame. Timeout of 1e9 ns
	VK_CHECK(vkWaitForFences(m_Device, 1, &GetCurrentFrame().renderFence, true, 1000000000));
	GetCurrentFrame().deletionQueue.flush();
	GetCurrentFrame().frameDescriptors.ClearPools(m_Device);
	VK_CHECK(vkResetFences(m_Device, 1, &GetCurrentFrame().renderFence));

	uint32_t swapchainImageIndex;
	VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_ResizeRequested = true;
		return;
	}

	VkCommandBuffer cmd = GetCurrentFrame().commandBuffer;
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	m_DrawExtent.width = std::min(m_SwapchainExtent.width, m_DrawImage.imageExtent.width) * m_RenderScale;
	m_DrawExtent.height = std::min(m_SwapchainExtent.height, m_DrawImage.imageExtent.height) * m_RenderScale;

	// Tell gpu that 1 submit per frame is happening so it optimizes for that
	VkCommandBufferBeginInfo cmdBeginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	VkUtils::transitionImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	DrawBackground(cmd);

	VkUtils::transitionImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkUtils::transitionImage(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	DrawGeometry(cmd);

	VkUtils::transitionImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	VkUtils::transitionImage(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkUtils::copyImageToImage(cmd, m_DrawImage.image, m_SwapchainImages[swapchainImageIndex], m_DrawExtent, m_SwapchainExtent);
	VkUtils::transitionImage(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	DrawImgui(cmd, m_SwapchainImageViews[swapchainImageIndex]);
	VkUtils::transitionImage(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = VkInit::commandBufferSubmitInfo(cmd);
	VkSemaphoreSubmitInfo waitInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);
	
	VkSubmitInfo2 submit = VkInit::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, GetCurrentFrame().renderFence));

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

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

	result = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_ResizeRequested = true;
	}

	m_FrameNumber++;
}

void VulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
	VK_CHECK(vkResetFences(m_Device, 1, &m_ImmediateFence));
	VK_CHECK(vkResetCommandBuffer(m_ImmediateCommandBuffer, 0));

	VkCommandBuffer cmd = m_ImmediateCommandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = VkInit::commandBufferSubmitInfo(cmd);
	VkSubmitInfo2 submit = VkInit::submitInfo(&cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, m_ImmediateFence));

	VK_CHECK(vkWaitForFences(m_Device, 1, &m_ImmediateFence, true, 9999999999));
}

void VulkanEngine::MainLoop()
{
	while (!glfwWindowShouldClose(m_Window))
	{
		glfwPollEvents();

		if (glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED))
		{
			// Window is minimized, skip rendering
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (m_ResizeRequested)
		{
			ResizeSwapchain();
		}

		// imgui new frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		//some imgui UI to test
		//ImGui::ShowDemoWindow();

		if (ImGui::Begin("background"))
		{
			ImGui::SliderFloat("Render Scale", &m_RenderScale, 0.3f, 1.f);

			ComputeEffect& selected = m_BGEffects[m_CurrentBGEffect];

			ImGui::Text("Selected effect: ", selected.name);
			ImGui::SliderInt("Effect Index", &m_CurrentBGEffect, 0, m_BGEffects.size() - 1);
			ImGui::ColorEdit4("data1", (float*)&selected.data.data1);
			ImGui::ColorEdit4("data2", (float*)&selected.data.data2);
			ImGui::ColorEdit4("data3", (float*)&selected.data.data3);
			ImGui::ColorEdit4("data4", (float*)&selected.data.data4);
		}
		ImGui::End();

		//make imgui calculate internal draw structures
		ImGui::Render();

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
		fmt::print("{} {}\n", \
			fmt::styled("GPU: ", fmt::fg(fmt::color::white) | fmt::emphasis::bold), \
			fmt::styled(deviceProperties.deviceName, fmt::fg(fmt::color::green_yellow)));\
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
	m_MainDeletionQueue.pushFunction([&]()
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

	// Add depth buffer
	m_DepthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
	m_DepthImage.imageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo depthImageInfo = VkInit::imageCreateInfo(m_DepthImage.imageFormat, depthImageUsages, drawImageExtent);
	vmaCreateImage(m_Allocator, &depthImageInfo, &imageAllocationInfo, &m_DepthImage.image, &m_DepthImage.allocation, nullptr);
	VkImageViewCreateInfo depthImageViewInfo = VkInit::imageviewCreateInfo(m_DepthImage.imageFormat, m_DepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
	VK_CHECK(vkCreateImageView(m_Device, &depthImageViewInfo, nullptr, &m_DepthImage.imageView));

	m_MainDeletionQueue.pushFunction([&]()
		{
			vkDestroyImageView(m_Device, m_DrawImage.imageView, nullptr);
			vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);

			vkDestroyImageView(m_Device, m_DepthImage.imageView, nullptr);
			vmaDestroyImage(m_Allocator, m_DepthImage.image, m_DepthImage.allocation);
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

	VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolInfo, nullptr, &m_ImmediateCommandPool));

	// allocate the command buffer for immediate submits
	VkCommandBufferAllocateInfo cmdAllocInfo = VkInit::commandBufferAllocateInfo(m_ImmediateCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(m_Device, &cmdAllocInfo, &m_ImmediateCommandBuffer));

	m_MainDeletionQueue.pushFunction([=]() {
		vkDestroyCommandPool(m_Device, m_ImmediateCommandPool, nullptr);
		});
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

	VK_CHECK(vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_ImmediateFence));
	m_MainDeletionQueue.pushFunction([=]() { vkDestroyFence(m_Device, m_ImmediateFence, nullptr); });
}

void VulkanEngine::InitDescriptors()
{
	//create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocatorDynamic::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
	};

	m_GlobalDescriptorAllocator.Init(m_Device, 10, sizes);

	//make the descriptor set layout for our compute draw
	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		m_DrawImageDescriptorLayout = builder.build(m_Device, VK_SHADER_STAGE_COMPUTE_BIT);
	}

	//allocate a descriptor set for our draw image
	m_DrawImageDescriptors = m_GlobalDescriptorAllocator.Allocate(m_Device, m_DrawImageDescriptorLayout);

	DescriptorWriter writer;
	writer.writeImage(0, m_DrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	writer.updateSet(m_Device, m_DrawImageDescriptors);

	//make sure both the descriptor allocator and the new layout get cleaned up properly
	m_MainDeletionQueue.pushFunction([&]() {
		m_GlobalDescriptorAllocator.DestroyPools(m_Device);
		vkDestroyDescriptorSetLayout(m_Device, m_DrawImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(m_Device, m_GPUSceneDataDescriptorLayout, nullptr);
	});

	// Send scene data to GPU
	{
		DescriptorLayoutBuilder builder;
		builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		m_GPUSceneDataDescriptorLayout = builder.build(m_Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		std::vector<DescriptorAllocatorDynamic::PoolSizeRatio> frameSizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		m_Frames[i].frameDescriptors = DescriptorAllocatorDynamic{};
		m_Frames[i].frameDescriptors.Init(m_Device, 1000, frameSizes);

		m_MainDeletionQueue.pushFunction([&, i]() {
			m_Frames[i].frameDescriptors.DestroyPools(m_Device);
		});
	}
}

void VulkanEngine::InitPipelines()
{
	// Compute
	InitBackgroundPipelines();

	// Graphics
	InitTrianglePipeline();
	InitMeshPipeline();
}

void VulkanEngine::InitBackgroundPipelines()
{
	VkPipelineLayoutCreateInfo computeLayout{};
	computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computeLayout.pNext = nullptr;
	computeLayout.pSetLayouts = &m_DrawImageDescriptorLayout;
	computeLayout.setLayoutCount = 1;

	VkPushConstantRange	pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(ComputePushConstants);
	pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	computeLayout.pPushConstantRanges = &pushConstant;
	computeLayout.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(m_Device, &computeLayout, nullptr, &m_GradientPipelineLayout));

	VkShaderModule gradientShader;
	if (!VkUtils::loadShaderModule(SHADER_PATH "gradient_color.comp.spv", m_Device, &gradientShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building gradient_color compute shader\n");
	}

	VkShaderModule skyShader;
	if (!VkUtils::loadShaderModule(SHADER_PATH "sky.comp.spv", m_Device, &skyShader))
	{	
		fmt::print(fmt::fg(fmt::color::red), "Error when building sky compute shader\n");
	}

	VkPipelineShaderStageCreateInfo stageinfo{};
	stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageinfo.pNext = nullptr;
	stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageinfo.module = gradientShader;
	stageinfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = m_GradientPipelineLayout;
	computePipelineCreateInfo.stage = stageinfo;

	ComputeEffect gradient;
	gradient.layout = m_GradientPipelineLayout;
	gradient.name = "gradient";
	gradient.data = {};
	gradient.data.data1 = glm::vec4(1, 0, 0, 1);
	gradient.data.data2 = glm::vec4(0, 0, 1, 1);

	VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

	//change the shader module only to create the sky shader
	computePipelineCreateInfo.stage.module = skyShader;

	ComputeEffect sky;
	sky.layout = m_GradientPipelineLayout;
	sky.name = "sky";
	sky.data = {};
	//default sky parameters
	sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

	VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

	//add the 2 background effects into the array
	m_BGEffects.push_back(gradient);
	m_BGEffects.push_back(sky);

	//destroy structures properly
	vkDestroyShaderModule(m_Device, gradientShader, nullptr);
	vkDestroyShaderModule(m_Device, skyShader, nullptr);
	m_MainDeletionQueue.pushFunction([=]()
		{
			vkDestroyPipelineLayout(m_Device, m_GradientPipelineLayout, nullptr);
			vkDestroyPipeline(m_Device, sky.pipeline, nullptr);
			vkDestroyPipeline(m_Device, gradient.pipeline, nullptr);
		});
}

void VulkanEngine::InitTrianglePipeline()
{
	VkShaderModule triangleFragShader;
	VkShaderModule triangleVertexShader;

	if (!VkUtils::loadShaderModule(SHADER_PATH "colored_triangle.frag.spv", m_Device, &triangleFragShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building colored_triangle frag shader\n");
	}
	if (!VkUtils::loadShaderModule(SHADER_PATH "colored_triangle.vert.spv", m_Device, &triangleVertexShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building colored_triangle vert shader\n");
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::pipelineLayoutCreateInfo();
	VK_CHECK(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_TrianglePipelineLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.m_PipelineLayout = m_TrianglePipelineLayout;
	pipelineBuilder.SetShaders(triangleVertexShader, triangleFragShader);
	pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.SetMultiSamplingNone();
	pipelineBuilder.DisableBlending();
	pipelineBuilder.DisableDepthTest();

	pipelineBuilder.SetColorAttachmentFormat(m_DrawImage.imageFormat);
	pipelineBuilder.SetDepthFormat(m_DepthImage.imageFormat);
	m_TrianglePipeline = pipelineBuilder.BuildPipeline(m_Device);

	vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);
	vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);

	m_MainDeletionQueue.pushFunction([&]() {
		vkDestroyPipelineLayout(m_Device, m_TrianglePipelineLayout, nullptr);
		vkDestroyPipeline(m_Device, m_TrianglePipeline, nullptr);
		});
}

void VulkanEngine::InitMeshPipeline()
{
	VkShaderModule triangleFragShader;
	VkShaderModule triangleVertexShader;

	if (!VkUtils::loadShaderModule(SHADER_PATH "colored_triangle.frag.spv", m_Device, &triangleFragShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building colored_triangle frag shader\n");
	}
	if (!VkUtils::loadShaderModule(SHADER_PATH "colored_triangle_mesh.vert.spv", m_Device, &triangleVertexShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building colored_triangle vert shader\n");
	}

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::pipelineLayoutCreateInfo();
	pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutInfo.pushConstantRangeCount = 1;

	VK_CHECK(vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_MeshPipelineLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.m_PipelineLayout = m_MeshPipelineLayout;
	pipelineBuilder.SetShaders(triangleVertexShader, triangleFragShader);
	pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.SetMultiSamplingNone();
	//pipelineBuilder.DisableBlending();
	pipelineBuilder.EnableBlendingAdditive();
	pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	//pipelineBuilder.DisableDepthTest();

	pipelineBuilder.SetColorAttachmentFormat(m_DrawImage.imageFormat);
	pipelineBuilder.SetDepthFormat(m_DepthImage.imageFormat);
	m_MeshPipeline = pipelineBuilder.BuildPipeline(m_Device);

	vkDestroyShaderModule(m_Device, triangleFragShader, nullptr);
	vkDestroyShaderModule(m_Device, triangleVertexShader, nullptr);

	m_MainDeletionQueue.pushFunction([&]() {
		vkDestroyPipelineLayout(m_Device, m_MeshPipelineLayout, nullptr);
		vkDestroyPipeline(m_Device, m_MeshPipeline, nullptr);
		});
}

void VulkanEngine::InitDefaultData()
{
	std::array<Vertex, 4> rectVertices;
	rectVertices[0].position = {  0.5f, -0.5f,  0.0f };
	rectVertices[1].position = {  0.5f,  0.5f,  0.0f };
	rectVertices[2].position = { -0.5f, -0.5f,  0.0f };
	rectVertices[3].position = { -0.5f,  0.5f,  0.0f };

	rectVertices[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	rectVertices[1].color = { 0.5f, 0.5f, 0.5f, 1.0f };
	rectVertices[2].color = { 1.0f, 0.0f, 0.0f, 1.0f };
	rectVertices[3].color = { 0.0f, 1.0f, 0.0f, 1.0f };

	std::array<uint32_t, 6> rectIndices{ 0, 1, 2, 2, 1, 3 };

	m_Rectangle = UploadMesh(rectIndices, rectVertices);

	m_MainDeletionQueue.pushFunction([&]() {
		DestroyBuffer(m_Rectangle.indexBuffer);
		DestroyBuffer(m_Rectangle.vertexBuffer);
		});

	m_TestMeshes = loadGltfMeshes(this, ASSET_PATH "basicmesh.glb").value();
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

void VulkanEngine::ResizeSwapchain()
{
	vkDeviceWaitIdle(m_Device);
	DestroySwapchain();

	int w, h;
	glfwGetWindowSize(m_Window, &w, &h);
	m_WindowExtent.width = w;
	m_WindowExtent.height = h;

	CreateSwapchain(m_WindowExtent.width, m_WindowExtent.height);
	m_ResizeRequested = false;
}

void VulkanEngine::DrawBackground(VkCommandBuffer& cmd)
{
	//VkClearColorValue clearValue;
	//float flash = std::abs(std::sin(m_FrameNumber / 120.f));
	//float flash2 = std::abs(std::sin(m_FrameNumber / 120.f + 20.0));
	//float flash3 = std::abs(std::sin(m_FrameNumber / 120.f + 40.0));
	//clearValue = { { flash, flash2, flash3, 1.0f } };
	//VkImageSubresourceRange clearRange = VkInit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
	//vkCmdClearColorImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	ComputeEffect& effect = m_BGEffects[m_CurrentBGEffect];

	// bind the gradient drawing compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_GradientPipelineLayout, 0, 1, &m_DrawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(cmd, m_GradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);
	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0f), std::ceil(m_DrawExtent.height / 16.0f), 1);
}

void VulkanEngine::DrawGeometry(VkCommandBuffer& cmd)
{
	VkRenderingAttachmentInfo colorAttachment = VkInit::attachmentInfo(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = VkInit::depthAttachmentInfo(m_DepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = VkInit::renderingInfo(m_DrawExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(cmd, &renderInfo);

	////////////////////////////////////////////////////////////////////////

	// Needs Triangle pipeline to be initialized
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_TrianglePipeline);

	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = m_DrawExtent.width;
	viewport.height = m_DrawExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = m_DrawExtent.width;
	scissor.extent.height = m_DrawExtent.height;

	vkCmdSetScissor(cmd, 0, 1, &scissor);

	// 3 vertices have to be drawns
	// Needs Triangle pipeline to be initialized
	vkCmdDraw(cmd, 3, 1, 0, 0);

	////////////////////////////////////////////////////////////////////////

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MeshPipeline);
	GPUDrawPushConstants pushConstants;
	
	pushConstants.worldMatrix = glm::mat4(1.0f);
	pushConstants.vertexBufferAddress = m_Rectangle.vertexDeviceAddress;

	vkCmdPushConstants(cmd, m_MeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);
	vkCmdBindIndexBuffer(cmd, m_Rectangle.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

	////////////////////////////////////////////////////////////////////////

	glm::mat4 view = glm::translate(glm::vec3{ 0, 0, -5 });
	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)m_DrawExtent.width / (float)m_DrawExtent.height, 10000.0f, 0.1f);
	projection[1][1] *= -1;

	pushConstants.worldMatrix = projection * view;
	pushConstants.vertexBufferAddress = m_TestMeshes[2]->meshBuffers.vertexDeviceAddress;

	vkCmdPushConstants(cmd, m_MeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);
	vkCmdBindIndexBuffer(cmd, m_TestMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(cmd, m_TestMeshes[2]->surfaces[0].count, 1, m_TestMeshes[2]->surfaces[0].startIndex, 0, 0);

	////////////////////////////////////////////////////////////////////////

	AllocatedBuffer gpuSceneDataBuffer = CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	GetCurrentFrame().deletionQueue.pushFunction([=, this]() {
		DestroyBuffer(gpuSceneDataBuffer);
	});

	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
	*sceneUniformData = m_SceneData;

	VkDescriptorSet globalDescriptor = GetCurrentFrame().frameDescriptors.Allocate(m_Device, m_GPUSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.writeBuffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.updateSet(m_Device, globalDescriptor);

	////////////////////////////////////////////////////////////////////////

	vkCmdEndRendering(cmd);
}

void VulkanEngine::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	VkRenderingAttachmentInfo colorAttachment = VkInit::attachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = VkInit::renderingInfo(m_SwapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
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

void VulkanEngine::InitImGui()
{
	// 1: create descriptor pool for IMGUI
	//  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = (uint32_t)std::size(pool_sizes);
	poolInfo.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &imguiPool));

	// 2: initialize imgui library

	// this initializes the core structures of imgui
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

	// this initializes imgui for SDL
	ImGui_ImplGlfw_InitForVulkan(m_Window, true);

	// this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = m_Instance;
	init_info.PhysicalDevice = m_PhysicalDevice;
	init_info.Device = m_Device;
	init_info.Queue = m_GraphicsQueue;
	init_info.DescriptorPool = imguiPool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.UseDynamicRendering = true;

	//dynamic rendering parameters for imgui to use
	init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_SwapchainFormat;

	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info);

	ImGui_ImplVulkan_CreateFontsTexture();

	// add the destroy the imgui created structures
	m_MainDeletionQueue.pushFunction([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(m_Device, imguiPool, nullptr);
		});
}

AllocatedBuffer VulkanEngine::CreateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
	VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.pNext = nullptr;
	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = memoryUsage;
	vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	AllocatedBuffer newBuffer;
	VK_CHECK(vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaAllocInfo,
		&newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));
	return newBuffer;
}

void VulkanEngine::DestroyBuffer(const AllocatedBuffer& buffer)
{
	vmaDestroyBuffer(m_Allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	newSurface.vertexBuffer = CreateBuffer(vertexBufferSize, 
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);
	newSurface.indexBuffer = CreateBuffer(indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	AllocatedBuffer staging = CreateBuffer(vertexBufferSize + indexBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	VkBufferDeviceAddressInfo deviceAddressInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = newSurface.vertexBuffer.buffer };
	newSurface.vertexDeviceAddress = vkGetBufferDeviceAddress(m_Device, &deviceAddressInfo);

	void* data = staging.allocation->GetMappedData();
	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	// This will block the CPU until GPU has finished executing so UploadMesh is generally called in separate thread
	ImmediateSubmit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
		});
	
	DestroyBuffer(staging);
	return newSurface;
}
