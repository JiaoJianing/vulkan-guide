
#pragma once 

#include <vulkan/vulkan.h>
#include <vk_types.h>
#include <fastgltf/types.hpp>

class VulkanEngine;

namespace vkutil {

	void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, uint32_t baseMipLevel = 0, uint32_t levelCount = VK_REMAINING_MIP_LEVELS);

	void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize, uint32_t srcMipLevel = 0, uint32_t dstMipLevel = 0);

	std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image);

	void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
};