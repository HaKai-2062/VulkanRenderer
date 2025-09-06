#pragma once

struct VkInstance_T;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkQueue_T;
struct VkDescriptorPool_T;
struct VkCommandBuffer_T;
using VkInstance = VkInstance_T*;
using VkPhysicalDevice = VkPhysicalDevice_T*;
using VkDevice = VkDevice_T*;
using VkQueue = VkQueue_T*;
using VkDescriptorPool = VkDescriptorPool_T*;
using VkCommandBuffer = VkCommandBuffer_T*;

class BackEndImGui
{
public:
	static void Init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VkQueue graphicsQueue, VkDescriptorPool imguiPool);
	static void BeginFrame();
	static void EndFrame(VkCommandBuffer cmd);
	static void Shutdown();

private:
	static void ImGuiDarkTheme();
};