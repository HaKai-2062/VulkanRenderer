#pragma once

#include "vk_types.h"

namespace VkUtils
{
	bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
}