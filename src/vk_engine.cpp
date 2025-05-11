#include <sstream>
#include <array>
#include <thread>
#include <chrono>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
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

static VkExtent2D ScreenSize{ 1920, 1080 };

bool isVisible(const RenderObject& obj, const glm::mat4& viewproj);

void VulkanEngine::Init()
{
	fmt::print(fmt::fg(fmt::color::green), "Application Created\n");

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_WindowExtent = ScreenSize;
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
		vkDeviceWaitIdle(Device);

		// Make sure GPU has stopped doing its things
		m_LoadedScenes.clear();

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroyCommandPool(Device, m_Frames[i].CommandPool, nullptr);

			vkDestroyFence(Device, m_Frames[i].RenderFence, nullptr);
			vkDestroySemaphore(Device, m_Frames[i].RenderSemaphore, nullptr);
			vkDestroySemaphore(Device, m_Frames[i].SwapchainSemaphore, nullptr);

			m_Frames[i].FrameDeletionQueue.Flush();
		}

		for (auto& mesh : m_TestMeshes)
		{
			DestroyBuffer(mesh->MeshBuffers.IndexBuffer);
			DestroyBuffer(mesh->MeshBuffers.VertexBuffer);
		}

		MetalRoughMaterial.ClearResources(Device);

		// Flush global deletion queue
		m_MainDeletionQueue.Flush();

		DestroySwapchain();
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

		vkDestroyDevice(Device, nullptr);
		vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger);

		glfwDestroyWindow(m_Window);
		vkDestroyInstance(m_Instance, nullptr);
		glfwTerminate();

		fmt::print(fmt::fg(fmt::color::yellow) | fmt::bg(fmt::color::black), "Application Destroyed\n");
	}
}

void VulkanEngine::MainLoop()
{
	while (!glfwWindowShouldClose(m_Window))
	{
		auto start = std::chrono::system_clock::now();

		glfwPollEvents();

		if (glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED))
		{
			// Window is minimized, skip rendering
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		double xPos, yPos;
		glfwGetCursorPos(m_Window, &xPos, &yPos);
		m_Camera.ProcessKeyEvents(m_Window, m_DeltaTime);
		m_Camera.ProcessMouseEvents(m_Window, xPos, yPos);

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

			glm::vec3 pos = m_Camera.GetCameraPosition();
			glm::vec3 rot = m_Camera.GetCameraOrientation();

			ImGui::Text("Frametime:   %f ms", Stats.FrameTime);
			ImGui::Text("Draw Time:   %f ms", Stats.MeshDrawTime);
			ImGui::Text("Update Time: %f ms", Stats.SceneUpdateTime);
			ImGui::Text("Triangles:   %i", Stats.TriangleCount);
			ImGui::Text("Draws:		  %i", Stats.DrawcallCount);

			ImGui::NewLine();

			ImGui::Text("Location:	  %f, %f, %f", pos.x, pos.y, pos.z);
			ImGui::Text("Rotation:	  %f, %f, %f", rot.x, rot.y, rot.z);
		}
		ImGui::End();

		//make imgui calculate internal draw structures
		ImGui::Render();

		UpdateDeltaTimeAndTitle();
		DrawFrame();

		auto end = std::chrono::system_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		Stats.FrameTime = elapsed.count() / 1000.f;
	}
}

void VulkanEngine::DrawFrame()
{
	UpdateScene();

	// Wait until the gpu has finished rendering the last frame. Timeout of 1e9 ns
	VK_CHECK(vkWaitForFences(Device, 1, &GetCurrentFrame().RenderFence, true, 1000000000));
	GetCurrentFrame().FrameDeletionQueue.Flush();
	GetCurrentFrame().FrameDescriptors.ClearPools(Device);
	VK_CHECK(vkResetFences(Device, 1, &GetCurrentFrame().RenderFence));

	uint32_t swapchainImageIndex;
	VkResult result = vkAcquireNextImageKHR(Device, m_Swapchain, 1000000000, GetCurrentFrame().SwapchainSemaphore, nullptr, &swapchainImageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_ResizeRequested = true;
		return;
	}

	VkCommandBuffer cmd = GetCurrentFrame().CommandBuffer;
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	m_DrawExtent.width = std::min(m_SwapchainExtent.width, DrawImage.ImageExtent.width) * m_RenderScale;
	m_DrawExtent.height = std::min(m_SwapchainExtent.height, DrawImage.ImageExtent.height) * m_RenderScale;

	// Tell gpu that 1 submit per frame is happening so it optimizes for that
	VkCommandBufferBeginInfo cmdBeginInfo = VkInit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	VkUtils::transitionImage(cmd, DrawImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkUtils::transitionImage(cmd, DepthImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	DrawMain(cmd);

	VkUtils::transitionImage(cmd, DrawImage.Image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	VkUtils::transitionImage(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	VkUtils::copyImageToImage(cmd, DrawImage.Image, m_SwapchainImages[swapchainImageIndex], m_DrawExtent, m_SwapchainExtent);
	VkUtils::transitionImage(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	DrawImgui(cmd, m_SwapchainImageViews[swapchainImageIndex]);
	VkUtils::transitionImage(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue. 
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = VkInit::commandBufferSubmitInfo(cmd);
	VkSemaphoreSubmitInfo waitInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().SwapchainSemaphore);
	VkSemaphoreSubmitInfo signalInfo = VkInit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().RenderSemaphore);

	VkSubmitInfo2 submit = VkInit::submitInfo(&cmdinfo, &signalInfo, &waitInfo);

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submit, GetCurrentFrame().RenderFence));

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
	presentInfo.pWaitSemaphores = &GetCurrentFrame().RenderSemaphore;
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
	VK_CHECK(vkResetFences(Device, 1, &m_ImmediateFence));
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

	VK_CHECK(vkWaitForFences(Device, 1, &m_ImmediateFence, true, 9999999999));
}

GPUMeshBuffers VulkanEngine::UploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
	const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
	const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

	GPUMeshBuffers newSurface;

	newSurface.VertexBuffer = CreateBuffer(vertexBufferSize,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY);
	newSurface.IndexBuffer = CreateBuffer(indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
	AllocatedBuffer staging = CreateBuffer(vertexBufferSize + indexBufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	VkBufferDeviceAddressInfo deviceAddressInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = newSurface.VertexBuffer.Buffer };
	newSurface.VertexDeviceAddress = vkGetBufferDeviceAddress(Device, &deviceAddressInfo);

	void* data = staging.Allocation->GetMappedData();
	memcpy(data, vertices.data(), vertexBufferSize);
	memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

	// This will block the CPU until GPU has finished executing so UploadMesh is generally called in separate thread
	ImmediateSubmit([&](VkCommandBuffer cmd) {
		VkBufferCopy vertexCopy{ 0 };
		vertexCopy.dstOffset = 0;
		vertexCopy.srcOffset = 0;
		vertexCopy.size = vertexBufferSize;

		vkCmdCopyBuffer(cmd, staging.Buffer, newSurface.VertexBuffer.Buffer, 1, &vertexCopy);

		VkBufferCopy indexCopy{ 0 };
		indexCopy.dstOffset = 0;
		indexCopy.srcOffset = vertexBufferSize;
		indexCopy.size = indexBufferSize;

		vkCmdCopyBuffer(cmd, staging.Buffer, newSurface.IndexBuffer.Buffer, 1, &indexCopy);
		});

	DestroyBuffer(staging);
	return newSurface;
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

	Device = vkbDevice.device;
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
	allocatorInfo.device = Device;
	allocatorInfo.instance = m_Instance;
	// To use GPU pointers
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaCreateAllocator(&allocatorInfo, &m_Allocator);
	m_MainDeletionQueue.PushFunction([&]()
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

	DrawImage.ImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	DrawImage.ImageExtent = drawImageExtent;
	
	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo imageInfo = VkInit::imageCreateInfo(DrawImage.ImageFormat, drawImageUsages, drawImageExtent);

	// Allocate GPU memory
	VmaAllocationCreateInfo imageAllocationInfo = {};
	imageAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	imageAllocationInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vmaCreateImage(m_Allocator, &imageInfo, &imageAllocationInfo, &DrawImage.Image, &DrawImage.Allocation, nullptr);
	VkImageViewCreateInfo imageviewInfo = VkInit::imageviewCreateInfo(DrawImage.ImageFormat, DrawImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);
	VK_CHECK(vkCreateImageView(Device, &imageviewInfo, nullptr, &DrawImage.ImageView));

	// Add depth buffer
	DepthImage.ImageFormat = VK_FORMAT_D32_SFLOAT;
	DepthImage.ImageExtent = drawImageExtent;
	VkImageUsageFlags depthImageUsages{};
	depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageCreateInfo depthImageInfo = VkInit::imageCreateInfo(DepthImage.ImageFormat, depthImageUsages, drawImageExtent);
	vmaCreateImage(m_Allocator, &depthImageInfo, &imageAllocationInfo, &DepthImage.Image, &DepthImage.Allocation, nullptr);
	VkImageViewCreateInfo depthImageViewInfo = VkInit::imageviewCreateInfo(DepthImage.ImageFormat, DepthImage.Image, VK_IMAGE_ASPECT_DEPTH_BIT);
	VK_CHECK(vkCreateImageView(Device, &depthImageViewInfo, nullptr, &DepthImage.ImageView));

	m_MainDeletionQueue.PushFunction([&]()
		{
			vkDestroyImageView(Device, DrawImage.ImageView, nullptr);
			vmaDestroyImage(m_Allocator, DrawImage.Image, DrawImage.Allocation);

			vkDestroyImageView(Device, DepthImage.ImageView, nullptr);
			vmaDestroyImage(m_Allocator, DepthImage.Image, DepthImage.Allocation);
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
		VK_CHECK(vkCreateCommandPool(Device, &commandPoolInfo, nullptr, &m_Frames[i].CommandPool));
		VkCommandBufferAllocateInfo cmdAllocInfo = VkInit::commandBufferAllocateInfo(m_Frames[i].CommandPool, 1);
		VK_CHECK(vkAllocateCommandBuffers(Device, &cmdAllocInfo, &m_Frames[i].CommandBuffer));
	}

	VK_CHECK(vkCreateCommandPool(Device, &commandPoolInfo, nullptr, &m_ImmediateCommandPool));

	// allocate the command buffer for immediate submits
	VkCommandBufferAllocateInfo cmdAllocInfo = VkInit::commandBufferAllocateInfo(m_ImmediateCommandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(Device, &cmdAllocInfo, &m_ImmediateCommandBuffer));

	m_MainDeletionQueue.PushFunction([=]() {
		vkDestroyCommandPool(Device, m_ImmediateCommandPool, nullptr);
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
		VK_CHECK(vkCreateFence(Device, &fenceCreateInfo, nullptr, &m_Frames[i].RenderFence));

		VK_CHECK(vkCreateSemaphore(Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].SwapchainSemaphore));
		VK_CHECK(vkCreateSemaphore(Device, &semaphoreCreateInfo, nullptr, &m_Frames[i].RenderSemaphore));
	}

	VK_CHECK(vkCreateFence(Device, &fenceCreateInfo, nullptr, &m_ImmediateFence));
	m_MainDeletionQueue.PushFunction([=]() { vkDestroyFence(Device, m_ImmediateFence, nullptr); });
}

void VulkanEngine::InitDescriptors()
{
	//create a descriptor pool that will hold 10 sets with 1 image each
	std::vector<DescriptorAllocatorDynamic::PoolSizeRatio> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
	};

	m_GlobalDescriptorAllocator.Init(Device, 10, sizes);

	// Set to send scene, light and cubemap data to GPU
	{
		DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		builder.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		//builder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		GPUSceneDataDescriptorLayout = builder.Build(Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	// Set to send mesh data to GPU
	{
		DescriptorLayoutBuilder builder;
		m_SingleImageDescriptorLayout = builder.Build(Device, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	// Set to send cubemap data to GPU
	{
		DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		m_CubeMapDescriptorLayout = builder.Build(Device, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	// Upload cubemap stuff
	if (!VkUtils::loadCubeMap(this, ASSET_PATH "cubemaps/cubemap_yokohama_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when trying to load cubemap\n");
		glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
	}

	// Allocate a descriptor set for our cubemap draw image and it is only sent once here
	m_CubeMapDescriptors = m_GlobalDescriptorAllocator.Allocate(Device, m_CubeMapDescriptorLayout);

	DescriptorWriter writer;
	writer.WriteImage(0, CubeMap.ImageView, CubeMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.UpdateSet(Device, m_CubeMapDescriptors);

	//make sure both the descriptor allocator and the new layout get cleaned up properly
	m_MainDeletionQueue.PushFunction([&]() {
		m_GlobalDescriptorAllocator.DestroyPools(Device);
		vkDestroyDescriptorSetLayout(Device, GPUSceneDataDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(Device, m_SingleImageDescriptorLayout, nullptr);
		vkDestroyDescriptorSetLayout(Device, m_CubeMapDescriptorLayout, nullptr);

		vkDestroySampler(Device, CubeMapSampler, nullptr);
		DestroyImage(CubeMap);
	});

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		std::vector<DescriptorAllocatorDynamic::PoolSizeRatio> frameSizes = {
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		};

		m_Frames[i].FrameDescriptors = DescriptorAllocatorDynamic{};
		m_Frames[i].FrameDescriptors.Init(Device, 1000, frameSizes);

		m_MainDeletionQueue.PushFunction([&, i]() {
			m_Frames[i].FrameDescriptors.DestroyPools(Device);
		});
	}
}

void VulkanEngine::InitPipelines()
{
	// Graphics
	InitCubeMapPipeline();
	InitMeshPipeline();

	MetalRoughMaterial.BuildPipelines(this);
}

void VulkanEngine::InitCubeMapPipeline()
{
	VkShaderModule fragShader;
	VkShaderModule vertexShader;

	if (!VkUtils::loadShaderModule(SHADER_PATH "cubemap.frag.spv", Device, &fragShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building cubemap frag shader\n");
	}
	if (!VkUtils::loadShaderModule(SHADER_PATH "cubemap.vert.spv", Device, &vertexShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building cubemap vert shader\n");
	}

	VkPushConstantRange bufferRange{};
	bufferRange.offset = 0;
	bufferRange.size = sizeof(GPUDrawPushConstants);
	bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkInit::pipelineLayoutCreateInfo();
	pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_CubeMapDescriptorLayout;
	pipelineLayoutInfo.setLayoutCount = 1;
	VK_CHECK(vkCreatePipelineLayout(Device, &pipelineLayoutInfo, nullptr, &m_CubeMapPipelineLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.PipelineLayout = m_CubeMapPipelineLayout;
	pipelineBuilder.SetShaders(vertexShader, fragShader);
	pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.SetMultiSamplingNone();
	pipelineBuilder.DisableBlending();
	//pipelineBuilder.EnableBlendingAdditive();
	//pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	pipelineBuilder.DisableDepthTest();

	//pipelineBuilder.SetColorAttachmentFormat(CubeMap.ImageFormat);
	// Depth format is necessary because in our draw we require it
	pipelineBuilder.SetDepthFormat(VK_FORMAT_D32_SFLOAT);
	m_CubeMapPipeline = pipelineBuilder.BuildPipeline(Device);

	vkDestroyShaderModule(Device, fragShader, nullptr);
	vkDestroyShaderModule(Device, vertexShader, nullptr);

	m_MainDeletionQueue.PushFunction([&]() {
		vkDestroyPipelineLayout(Device, m_CubeMapPipelineLayout, nullptr);
		vkDestroyPipeline(Device, m_CubeMapPipeline, nullptr);
		});
}

void VulkanEngine::InitMeshPipeline()
{
	VkShaderModule fragShader;
	VkShaderModule vertexShader;

	if (!VkUtils::loadShaderModule(SHADER_PATH "mesh.frag.spv", Device, &fragShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building tex_image frag shader\n");
	}
	if (!VkUtils::loadShaderModule(SHADER_PATH "mesh.vert.spv", Device, &vertexShader))
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
	pipelineLayoutInfo.pSetLayouts = &m_SingleImageDescriptorLayout;
	pipelineLayoutInfo.setLayoutCount = 1;
	VK_CHECK(vkCreatePipelineLayout(Device, &pipelineLayoutInfo, nullptr, &m_MeshPipelineLayout));

	PipelineBuilder pipelineBuilder;
	pipelineBuilder.PipelineLayout = m_MeshPipelineLayout;
	pipelineBuilder.SetShaders(vertexShader, fragShader);
	pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.SetMultiSamplingNone();
	pipelineBuilder.DisableBlending();
	//pipelineBuilder.EnableBlendingAdditive();
	pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
	//pipelineBuilder.DisableDepthTest();

	pipelineBuilder.SetColorAttachmentFormat(DrawImage.ImageFormat);
	pipelineBuilder.SetDepthFormat(DepthImage.ImageFormat);
	m_MeshPipeline = pipelineBuilder.BuildPipeline(Device);

	vkDestroyShaderModule(Device, fragShader, nullptr);
	vkDestroyShaderModule(Device, vertexShader, nullptr);

	m_MainDeletionQueue.PushFunction([&]() {
		vkDestroyPipelineLayout(Device, m_MeshPipelineLayout, nullptr);
		vkDestroyPipeline(Device, m_MeshPipeline, nullptr);
		});
}

void VulkanEngine::InitDefaultData()
{
	std::array<Vertex, 8> cubeVertices;
	cubeVertices[0].Position = {  1.0f, -1.0f,  1.0f, };
	cubeVertices[1].Position = {  1.0f,  1.0f,  1.0f, };
	cubeVertices[2].Position = { -1.0f, -1.0f,  1.0f, };
	cubeVertices[3].Position = { -1.0f,  1.0f,  1.0f, };
	cubeVertices[4].Position = {  1.0f, -1.0f, -1.0f, };
	cubeVertices[5].Position = {  1.0f,  1.0f, -1.0f, };
	cubeVertices[6].Position = { -1.0f, -1.0f, -1.0f, };
	cubeVertices[7].Position = { -1.0f,  1.0f, -1.0f, };

	for (uint32_t i = 0; i < 8; i++)
		cubeVertices[i].Color = { 1.0f, 1.0f, 1.0f, 1.0f };

	std::array<uint32_t, 36> cubeIndices = {
		// Front face
		0, 1, 2, 2, 1, 3,
		// Back face
		4, 5, 6, 6, 5, 7,
		// Left face
		4, 0, 6, 6, 0, 2,
		// Right face
		1, 5, 3, 3, 5, 7,
		// Top face
		1, 0, 5, 5, 0, 4,
		// Bottom face
		2, 3, 6, 6, 3, 7
	};

	m_Cube = UploadMesh(cubeIndices, cubeVertices);

	m_MainDeletionQueue.PushFunction([&]() {
		DestroyBuffer(m_Cube.IndexBuffer);
		DestroyBuffer(m_Cube.VertexBuffer);
		});

	//m_TestMeshes = LoadGltfMeshes(this, ASSET_PATH "basicmesh.glb").value();

	// Default textures to fallback to if a texture is not provided in the pipeline
	uint32_t black = glm::packUnorm4x8(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
	uint32_t white = glm::packUnorm4x8(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
	WhiteImage = UploadImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
	uint32_t purple = glm::packUnorm4x8(glm::vec4(0.5f, 0.5f, 1.0f, 1.0f));
	PurpleImage = UploadImage((void*)&purple, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	//uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 0.66f));
	//m_GreyImage = UploadImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
	//m_BlackImage = UploadImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

	// Checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
	std::array<uint32_t, 16*16> pixels;
	for (uint8_t x = 0; x < 16; x++)
	{
		for (uint8_t y = 0; y < 16; y++)
		{
			pixels[y*16+x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	
	ErrorCheckerboardImage = UploadImage(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
	
	VkSamplerCreateInfo sampler = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampler.magFilter = VK_FILTER_NEAREST;
	sampler.minFilter = VK_FILTER_NEAREST;
	vkCreateSampler(Device, &sampler, nullptr, &m_DefaultSamplerNearest);
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(Device, &sampler, nullptr, &DefaultSamplerLinear);

	std::string structurePath = { ASSET_PATH "Sponza/Sponza.gltf" };
	auto structureFile = loadGltfScene(this, structurePath);
	assert(structureFile.has_value());
	m_LoadedScenes["structure"] = *structureFile;

	m_MainDeletionQueue.PushFunction([&]() {
		vkDestroySampler(Device, m_DefaultSamplerNearest, nullptr),
		vkDestroySampler(Device, DefaultSamplerLinear, nullptr),

		DestroyImage(WhiteImage);
		DestroyImage(PurpleImage);
		//DestroyImage(m_GreyImage);
		//DestroyImage(m_BlackImage);
		DestroyImage(ErrorCheckerboardImage);
		});
}

void GLTFMetallic_Roughness::BuildPipelines(VulkanEngine* engine)
{
	VkShaderModule fragShader;
	VkShaderModule vertexShader;

	if (!VkUtils::loadShaderModule(SHADER_PATH "scene.frag.spv", engine->Device, &fragShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building the scene fragment shader\n");
	}
	if (!VkUtils::loadShaderModule(SHADER_PATH "scene.vert.spv", engine->Device, &vertexShader))
	{
		fmt::print(fmt::fg(fmt::color::red), "Error when building the scene vertex shader\n");
	}

	VkPushConstantRange matrixRange{};
	matrixRange.offset = 0;
	matrixRange.size = sizeof(GPUDrawPushConstants);
	matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.AddBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.AddBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.AddBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	MaterialLayout = layoutBuilder.Build(engine->Device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layouts[] = { engine->GPUSceneDataDescriptorLayout, MaterialLayout };

	VkPipelineLayoutCreateInfo meshLayoutInfo = VkInit::pipelineLayoutCreateInfo();
	meshLayoutInfo.setLayoutCount = 2;
	meshLayoutInfo.pSetLayouts = layouts;
	meshLayoutInfo.pPushConstantRanges = &matrixRange;
	meshLayoutInfo.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout;
	VK_CHECK(vkCreatePipelineLayout(engine->Device, &meshLayoutInfo, nullptr, &newLayout));

	OpaquePipeline.Layout = newLayout;
	TransparentPipeline.Layout = newLayout;

	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.SetShaders(vertexShader, fragShader);
	pipelineBuilder.SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	pipelineBuilder.SetMultiSamplingNone();
	pipelineBuilder.DisableBlending();
	pipelineBuilder.EnableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//render format
	pipelineBuilder.SetColorAttachmentFormat(engine->DrawImage.ImageFormat);
	pipelineBuilder.SetDepthFormat(engine->DepthImage.ImageFormat);

	// use the triangle layout we created
	pipelineBuilder.PipelineLayout = newLayout;

	// finally build the pipeline
	OpaquePipeline.Pipeline = pipelineBuilder.BuildPipeline(engine->Device);

	// create the transparent variant
	pipelineBuilder.EnableBlendingAdditive();

	pipelineBuilder.EnableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	TransparentPipeline.Pipeline = pipelineBuilder.BuildPipeline(engine->Device);

	vkDestroyShaderModule(engine->Device, fragShader, nullptr);
	vkDestroyShaderModule(engine->Device, vertexShader, nullptr);
}

void GLTFMetallic_Roughness::ClearResources(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, MaterialLayout, nullptr);
	vkDestroyPipelineLayout(device, TransparentPipeline.Layout, nullptr);

	vkDestroyPipeline(device, TransparentPipeline.Pipeline, nullptr);
	vkDestroyPipeline(device, OpaquePipeline.Pipeline, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorDynamic& descriptorAllocator)
{
	MaterialInstance matData;
	matData.PassType = pass;
	if (pass == MaterialPass::Transparent)
	{
		matData.Pipeline = &TransparentPipeline;
	}
	else
	{
		matData.Pipeline = &OpaquePipeline;
	}

	matData.MaterialSet = descriptorAllocator.Allocate(device, MaterialLayout);

	Writer.Clear();
	Writer.WriteBuffer(0, resources.DataBuffer, sizeof(MaterialConstants), resources.DataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	Writer.WriteImage(1, resources.ColorImage.ImageView, resources.ColorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	Writer.WriteImage(2, resources.MetalRoughImage.ImageView, resources.MetalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	Writer.WriteImage(3, resources.AOImage.ImageView, resources.AOSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	Writer.WriteImage(4, resources.NormalMapImage.ImageView, resources.NormalMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	Writer.UpdateSet(device, matData.MaterialSet);

	return matData;
}

void VulkanEngine::CreateSwapchain(uint32_t width, uint32_t height)
{
	vkb::SwapchainBuilder swapchainBuilder{ m_PhysicalDevice, Device, m_Surface };
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
	vkDestroySwapchainKHR(Device, m_Swapchain, nullptr);

	for (int i = 0; i < m_SwapchainImageViews.size(); i++)
	{
		vkDestroyImageView(Device, m_SwapchainImageViews[i], nullptr);
	}
}

void VulkanEngine::ResizeSwapchain()
{
	vkDeviceWaitIdle(Device);
	
	DestroySwapchain();

	int w, h;
	glfwGetWindowSize(m_Window, &w, &h);
	m_WindowExtent.width = w;
	m_WindowExtent.height = h;

	CreateSwapchain(m_WindowExtent.width, m_WindowExtent.height);
	m_ResizeRequested = false;
}

void VulkanEngine::DrawMesh(VkCommandBuffer cmd)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MeshPipeline);
	
	for (uint32_t i = 0; i < m_Lights.TotalPointLights; i++)
	{
		GPUDrawPushConstants pushConstants;
		glm::mat4 model = glm::translate(m_Lights.PointLights[i].Position) * glm::scale(glm::vec3(0.2f));
		pushConstants.WorldMatrix = m_SceneData.ViewProj * model;
		pushConstants.VertexBufferAddress = m_Cube.VertexDeviceAddress;

		vkCmdPushConstants(cmd, m_MeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);
		vkCmdBindIndexBuffer(cmd, m_Cube.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
	}
}

void VulkanEngine::DrawCubeMap(VkCommandBuffer cmd)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CubeMapPipeline);

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

	GPUDrawPushConstants pushConstants;
	glm::mat4 model = glm::identity<glm::mat4>();
	// We need to remove translation from the view matrix so mat4->mat3->mat4
	pushConstants.WorldMatrix = m_SceneData.Proj * glm::mat4(glm::mat3(m_SceneData.View)) * model;
	pushConstants.VertexBufferAddress = m_Cube.VertexDeviceAddress;

	vkCmdPushConstants(cmd, m_CubeMapPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);
	vkCmdBindIndexBuffer(cmd, m_Cube.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CubeMapPipelineLayout, 0, 1, &m_CubeMapDescriptors, 0, nullptr);

	vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);
}

void VulkanEngine::DrawMain(VkCommandBuffer cmd)
{
	////////////////////////////////////////////////
	
	VkRenderingAttachmentInfo colorAttachment = VkInit::attachmentInfo(DrawImage.ImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachment = VkInit::depthAttachmentInfo(DepthImage.ImageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = VkInit::renderingInfo(m_DrawExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(cmd, &renderInfo);
	
	auto start = std::chrono::system_clock::now();

	DrawCubeMap(cmd);
	DrawGeometry(cmd);
	DrawMesh(cmd);

	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	Stats.MeshDrawTime = elapsed.count() / 1000.0f;

	vkCmdEndRendering(cmd);
}

void VulkanEngine::DrawGeometry(VkCommandBuffer cmd)
{
	std::vector<uint32_t> opaqueDraws;
	opaqueDraws.reserve(m_MainDrawContext.OpaqueSurfaces.size());

	for (uint32_t i = 0; i < m_MainDrawContext.OpaqueSurfaces.size(); i++)
	{
		if (isVisible(m_MainDrawContext.OpaqueSurfaces[i], m_SceneData.ViewProj))
		{
			opaqueDraws.push_back(i);
		}
	}

	// Sort the opaque surfaces by material and mesh
	std::sort(opaqueDraws.begin(), opaqueDraws.end(), [&](const auto& iA, const auto& iB)
		{
		const RenderObject& A = m_MainDrawContext.OpaqueSurfaces[iA];
		const RenderObject& B = m_MainDrawContext.OpaqueSurfaces[iB];
		if (A.Material == B.Material)
		{
			return A.IndexBuffer < B.IndexBuffer;
		}
		else
		{
			return A.Material < B.Material;
		}
		});

	AllocatedBuffer gpuSceneDataBuffer = CreateBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	AllocatedBuffer lightDataBuffer = CreateBuffer(sizeof(LightData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	GetCurrentFrame().FrameDeletionQueue.PushFunction([=, this]() {
		DestroyBuffer(gpuSceneDataBuffer);
		DestroyBuffer(lightDataBuffer);
		});

	GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.Allocation->GetMappedData();
	*sceneUniformData = m_SceneData;
	LightData* lightUniformData = (LightData*)lightDataBuffer.Allocation->GetMappedData();
	*lightUniformData = m_Lights;

	VkDescriptorSet sceneDescriptor = GetCurrentFrame().FrameDescriptors.Allocate(Device, GPUSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.WriteBuffer(0, gpuSceneDataBuffer.Buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.WriteBuffer(1, lightDataBuffer.Buffer, sizeof(LightData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	//writer.WriteImage(2, CubeMap.ImageView, CubeMapSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.UpdateSet(Device, sceneDescriptor);

	// Defined outside of the draw function, this is the state we will try to skip
	MaterialPipeline* lastPipeline = nullptr;
	MaterialInstance* lastMaterial = nullptr;
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

	Stats.DrawcallCount = 0;
	Stats.TriangleCount = 0;

	auto drawLambda = [&](const RenderObject& draw)
		{
		if (draw.Material != lastMaterial)
		{
			lastMaterial = draw.Material;
			if (draw.Material->Pipeline != lastPipeline)
			{
				lastPipeline = draw.Material->Pipeline;
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.Material->Pipeline->Pipeline);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.Material->Pipeline->Layout, 0, 1, 
					&sceneDescriptor, 0, nullptr);
			
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
			}
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.Material->Pipeline->Layout, 1, 1, 
				&draw.Material->MaterialSet, 0, nullptr);
		}

		if (draw.IndexBuffer != lastIndexBuffer)
		{
			lastIndexBuffer = draw.IndexBuffer;
			vkCmdBindIndexBuffer(cmd, draw.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}

		// Calculate final mesh matrix
		GPUDrawPushConstants pushConstants;
		pushConstants.VertexBufferAddress = draw.VertexBufferAddress;
		pushConstants.WorldMatrix = draw.Transform;
		vkCmdPushConstants(cmd, draw.Material->Pipeline->Layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);
		vkCmdDrawIndexed(cmd, draw.IndexCount, 1, draw.FirstIndex, 0, 0);
		
		Stats.DrawcallCount++;
		Stats.TriangleCount = draw.IndexCount / 3;
		};

	for (auto& r : opaqueDraws)
	{
		drawLambda(m_MainDrawContext.OpaqueSurfaces[r]);
	}

	for (auto& r : m_MainDrawContext.TransparentSurfaces)
	{
		drawLambda(r);
	}

	m_MainDrawContext.OpaqueSurfaces.clear();
	m_MainDrawContext.TransparentSurfaces.clear();
}

void VulkanEngine::DrawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
	VkRenderingAttachmentInfo colorAttachment = VkInit::attachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingInfo renderInfo = VkInit::renderingInfo(m_SwapchainExtent, &colorAttachment, nullptr);

	vkCmdBeginRendering(cmd, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

	vkCmdEndRendering(cmd);
}

void VulkanEngine::UpdateDeltaTimeAndTitle()
{
	// Update Title
	static float lastTime = 0.0f;
	static uint32_t nFrames = 0;

	float currentTime = static_cast<float>(glfwGetTime());
	m_TitleUpdateTime = currentTime - lastTime;
	nFrames++;

	if (m_TitleUpdateTime >= 1.0)
	{
		uint32_t fps = static_cast<uint32_t>(nFrames / m_TitleUpdateTime);

		float delay = static_cast<uint32_t>(100'000.0f / nFrames) / 100.0f;

		std::stringstream ss;
		ss << "Vulkan Engine" << "    [FPS: " << fps << "]     " << "[" << delay << " ms]";
		glfwSetWindowTitle(m_Window, ss.str().c_str());

		nFrames = 0;
		lastTime = currentTime;
	}

	// Update delta
	m_DeltaTime = currentTime - m_LastFrameTime;
	m_LastFrameTime = currentTime;
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
	VK_CHECK(vkCreateDescriptorPool(Device, &poolInfo, nullptr, &imguiPool));

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
	init_info.Device = Device;
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
	m_MainDeletionQueue.PushFunction([=]() {
		ImGui_ImplVulkan_Shutdown();
		vkDestroyDescriptorPool(Device, imguiPool, nullptr);
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
		&newBuffer.Buffer, &newBuffer.Allocation, &newBuffer.Info));
	return newBuffer;
}

void VulkanEngine::DestroyBuffer(const AllocatedBuffer& buffer)
{
	vmaDestroyBuffer(m_Allocator, buffer.Buffer, buffer.Allocation);
}

AllocatedImage VulkanEngine::CreateImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	AllocatedImage newImage;
	newImage.ImageFormat = format;
	newImage.ImageExtent = size;

	VkImageCreateInfo imageInfo = VkInit::imageCreateInfo(format, usage, size);
	if (mipmapped)
	{
		imageInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
	}

	// Allocate memory on GPU
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(m_Allocator, &imageInfo, &allocInfo, &newImage.Image, &newImage.Allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT)
	{
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	VkImageViewCreateInfo viewInfo = VkInit::imageviewCreateInfo(format, newImage.Image, aspectFlag);
	viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
	VK_CHECK(vkCreateImageView(Device, &viewInfo, nullptr, &newImage.ImageView));

	return newImage;
}

AllocatedImage VulkanEngine::UploadImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
	// RGBA = 4 channels so multiplied by 4 to get 4 bytes cuz VK_FORMAT_R8G8B8A8_UNORM
	size_t dataSize = size.depth * size.width * size.height * 4;

	AllocatedBuffer uploadBuffer = CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	memcpy(uploadBuffer.Info.pMappedData, data, dataSize);

	AllocatedImage newImage = CreateImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

	ImmediateSubmit([&](VkCommandBuffer cmd)
	{
		VkUtils::transitionImage(cmd, newImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent = size;

		vkCmdCopyBufferToImage(cmd, uploadBuffer.Buffer, newImage.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
		if (mipmapped)
		{
			VkUtils::generateMipmaps(cmd, newImage.Image, VkExtent2D{ newImage.ImageExtent.width, newImage.ImageExtent.height });
		}
		else
		{
			VkUtils::transitionImage(cmd, newImage.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	});

	DestroyBuffer(uploadBuffer);
	return newImage;
}

AllocatedImage VulkanEngine::CreateCubeMapImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, uint32_t mipMapLevels)
{
	AllocatedImage newImage;
	newImage.ImageFormat = format;
	newImage.ImageExtent = size;

	VkImageCreateInfo imageInfo = VkInit::imageCreateInfo(format, usage, size);
	imageInfo.mipLevels = mipMapLevels;
	imageInfo.arrayLayers = 6;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	// Allocate memory on GPU
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CHECK(vmaCreateImage(m_Allocator, &imageInfo, &allocInfo, &newImage.Image, &newImage.Allocation, nullptr));

	// if the format is a depth format, we will need to have it use the correct aspect flag
	VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
	if (format == VK_FORMAT_D32_SFLOAT)
	{
		aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	VkImageViewCreateInfo viewInfo = VkInit::imageviewCreateInfo(format, newImage.Image, aspectFlag);
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewInfo.subresourceRange.layerCount = 6;
	viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
	VK_CHECK(vkCreateImageView(Device, &viewInfo, nullptr, &newImage.ImageView));

	return newImage;
}

AllocatedImage VulkanEngine::UploadCubeMapImage(void* data, const std::span<VkBufferImageCopy> bufferCopyRegions,
	VkExtent3D size, VkFormat format, VkImageUsageFlags usage,	uint32_t mipMapLevels, size_t dataSize)
{
	AllocatedBuffer uploadBuffer = CreateBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	memcpy(uploadBuffer.Info.pMappedData, data, dataSize);

	AllocatedImage newImage = CreateCubeMapImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipMapLevels);

	ImmediateSubmit([&](VkCommandBuffer cmd)
		{
			VkUtils::transitionImage(cmd, newImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
				mipMapLevels, 6);

			vkCmdCopyBufferToImage(cmd, uploadBuffer.Buffer, newImage.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());
			{
				VkUtils::transitionImage(cmd, newImage.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
		});

	DestroyBuffer(uploadBuffer);
	return newImage;
}

void VulkanEngine::DestroyImage(const AllocatedImage& image)
{
	vkDestroyImageView(Device, image.ImageView, nullptr);
	vmaDestroyImage(m_Allocator, image.Image, image.Allocation);
}

void VulkanEngine::UpdateScene()
{
	auto start = std::chrono::system_clock::now();

	//m_LoadedNodes["Suzanne"]->Draw(glm::mat4(1.0f), m_MainDrawContext);

	m_SceneData.View = m_Camera.GetViewMatrix();
	m_SceneData.Proj = glm::perspective(glm::radians(70.f), (float)m_WindowExtent.width / (float)m_WindowExtent.height, 10000.f, 0.1f);
	m_SceneData.Proj[1][1] *= -1;
	m_SceneData.ViewProj = m_SceneData.Proj * m_SceneData.View;

	// Default lighting parameters
	m_SceneData.AmbientColor = glm::vec4(0.1f);
	m_SceneData.SunlightColor = glm::vec4(1.0f);
	m_SceneData.SunlightDirection = glm::vec4(0.0f, 1.0f, 0.5f, 1.0f);

	m_SceneData.CameraPosition = glm::vec4(m_Camera.GetCameraPosition(), 1.0);
	m_SceneData.Time = glfwGetTime();

	float lightSpeed = 1.0f;
	std::vector<glm::vec3> lightLocations = { glm::vec3(-8.0f, 8.5f, 0.0f), glm::vec3(33.0f, 8.5f, 0.0f) };
	for (uint32_t i = 0; i < lightLocations.size(); i++)
	{
		PointLight light = {};
		light.Position = lightLocations[i] + glm::vec3(8.0f * sin(glfwGetTime() * lightSpeed), 0.0f, 9.0f * cos(glfwGetTime() * lightSpeed));
		light.Radius = 1.0f;
		light.Color = glm::vec3(1.0f, 1.0f, 1.0f);
		light.Intensity = 255.0f;

		m_Lights.PointLights[i] = light;
	}
	m_Lights.TotalPointLights = lightLocations.size();

	//for (int x = -3; x < 3; x++)
	//{

	//	glm::mat4 scale = glm::scale(glm::vec3{ 0.2f });
	//	glm::mat4 translation = glm::translate(glm::vec3{ x, 1.0f, 0.0f });

	//	m_LoadedNodes["Cube"]->Draw(translation * scale, m_MainDrawContext);
	//}

	m_LoadedScenes["structure"]->Draw(glm::mat4{ 1.0f }, m_MainDrawContext);

	auto end = std::chrono::system_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	Stats.SceneUpdateTime = elapsed.count() / 1000.f;
}

void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
	glm::mat4 nodeMatrix = topMatrix * WorldTransform;

	for (auto& surface : Mesh->Surfaces)
	{
		RenderObject obj;
		obj.IndexCount = surface.Count;
		obj.FirstIndex = surface.StartIndex;
		obj.IndexBuffer = Mesh->MeshBuffers.IndexBuffer.Buffer;
		obj.Material = &surface.Material->Data;
		
		obj.BoundingBox = surface.BoundingBox;
		obj.Transform = nodeMatrix;
		obj.VertexBufferAddress = Mesh->MeshBuffers.VertexDeviceAddress;

		if (surface.Material->Data.PassType == MaterialPass::Transparent)
		{
			ctx.TransparentSurfaces.push_back(obj);
		}
		else
		{
			ctx.OpaqueSurfaces.push_back(obj);
		}
	}

	// Recurse down
	Node::Draw(topMatrix, ctx);
}

bool isVisible(const RenderObject& obj, const glm::mat4& viewproj)
{
	std::array<glm::vec3, 8> corners
	{
		glm::vec3 { 1, 1, 1 },
		glm::vec3 { 1, 1, -1 },
		glm::vec3 { 1, -1, 1 },
		glm::vec3 { 1, -1, -1 },
		glm::vec3 { -1, 1, 1 },
		glm::vec3 { -1, 1, -1 },
		glm::vec3 { -1, -1, 1 },
		glm::vec3 { -1, -1, -1 },
	};

	glm::mat4 matrix = viewproj * obj.Transform;

	glm::vec3 min = { 1.5, 1.5, 1.5 };
	glm::vec3 max = { -1.5, -1.5, -1.5 };

	for (int c = 0; c < 8; c++)
	{
		// project each corner into clip space
		glm::vec4 v = matrix * glm::vec4(obj.BoundingBox.Origin + (corners[c] * obj.BoundingBox.Extents), 1.0f);

		// perspective correction
		v.x = v.x / v.w;
		v.y = v.y / v.w;
		v.z = v.z / v.w;

		min = glm::min(glm::vec3{ v.x, v.y, v.z }, min);
		max = glm::max(glm::vec3{ v.x, v.y, v.z }, max);
	}

	// check the clip space box is within the view
	if (min.z > 1.0f || max.z < 0.0f || min.x > 1.0f || max.x < -1.0f || min.y > 1.0f || max.y < -1.0f)
	{
		return false;
	}
	else
	{
		return true;
	}
}
