#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;

	// 添加一个绑定点
	void add_binding(uint32_t binding, VkDescriptorType type);
	void clear();
	// 构建一个描述符集布局
	VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator
{
	struct PoolSizeRatio
	{
		VkDescriptorType type;
		float ratio;
	};

	// 通过描述符池 + 描述符集布局分配描述符集
	VkDescriptorPool pool;

	void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
	void clear_descriptors(VkDevice device);
	void destroy_pool(VkDevice device);

	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

// 可以动态增长的描述符分配器
// 如果已分配的pool在分配描述符集时失败，会创建新的pool来重新分配
struct DescriptorAllocatorGrowable
{
public:
	struct PoolSizeRatio
	{
		VkDescriptorType type;
		float ratio;
	};

	// 初始化，创建第一个pool并放入readyPools中
	void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
	//  清空所有pool，将所有pool都放入readyPools中
	void clear_pools(VkDevice device);
	// 销毁所有pool
	void destroy_pools(VkDevice device);

	// 尝试分配一个描述符集
	// 调用get_pool获取可用的pool，若分配失败会把pool放入fullPools，然后再次调用get_pool
	// 若仍然失败则报错退出，若成功则将最新的pool放入readyPools
	VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);

private:
	// 尝试获取一个可用的pool
	// 会先从reayPools中获取，若没有可用的，会创建一个新的
	VkDescriptorPool get_pool(VkDevice device);
	VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> ratios;
	// 已经满载的pool
	std::vector<VkDescriptorPool> fullPools;
	// 准备好的，可用的pool
	std::vector<VkDescriptorPool> readyPools;
	// pool内可以装载多少描述符，若描述符集申请失败，会在创建新的pool时扩大setsPool为原来的1.5倍(最大到4092个)
	uint32_t setsPerPool;
};

struct DescriptorWriter
{
	std::deque<VkDescriptorImageInfo> imageInfos;
	std::deque<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkWriteDescriptorSet> writes;

	// 可用的type类型包括:
	// VK_DESCRIPTOR_TYPE_SAMPLER 一个采样器 只需要填充sampler
	// VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE 指向一张纹理，不需要填充sampler
	// VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER 包括采样器的纹理，imageView、sampler、layout都要填充
	// VK_DESCRIPTOR_TYPE_STORAGE_IMAGE 可读可写的纹理，不用填充sampler
	void write_image(int binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
	// 可用的type类型包括:
	// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	// VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
	// VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
	// VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
	void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

	void clear();
	void update_set(VkDevice device, VkDescriptorSet set);
};