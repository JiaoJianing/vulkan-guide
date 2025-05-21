// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>
#include <vk_loader.h>

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function)
	{
		deletors.push_back(function);
	}

	void flush()
	{
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
		{
			(*it)();
		}

		deletors.clear();
	}
};

// 帧数据 例如可以在CPU端记录多帧的渲染命令 保持CPU和GPU都忙碌的状态
struct FrameData
{
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;

	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	VkFence _renderFence;

	DeletionQueue _deletionQueue;
	// 用于动态分配描述符集
	DescriptorAllocatorGrowable _frameDescriptors;
};

// 本示例使用2个帧资源 相当于CPU最多可以同时录制2帧的渲染命令
constexpr unsigned int FRAME_OVERLAP = 2;

struct ComputePushConstants
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

// 用于确定使用哪个计算着色器
struct ComputeEffect
{
	const char* name;
	VkPipeline pipeline;
	VkPipelineLayout layout;
	ComputePushConstants data;
};

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	void draw_background(VkCommandBuffer cmd);

	void draw_geometry(VkCommandBuffer cmd);

	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	//run main loop
	void run();

	// 可以通过这个函数将cpu顶点上传到gpu
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	// 将Mesh的顶点信息上传到gpu 在渲染阶段可以实现不绑定VertexBuffer 直接将顶点的GPU地址通过push-constant传到shader
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	// 创建/销毁纹理
	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	// 通过一个stagingBuffer将像素数据上传到gpu
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage& img);

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; }

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

public:
	DeletionQueue _mainDeletionQueue;
	// 用于分配buffer和image的显存分配器
	VmaAllocator _allocator;

	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	// drawImage和depthImage在本示例中不会动态重建 它们的尺寸在初始化时确定
	// 当窗口大小变化时：若窗口比image小，drawImage只绘制窗口大小的区域；若窗口比image大，drawImage会放大
	// drawExtent区域由窗口大小、image大小、renderScale控制
	VkExtent2D _drawExtent;
	float _renderScale = 1.0f;

	// 窗口尺寸变化后，SwapChain需要重建
	bool _resize_requested = false;

	// 计算着色器绘制背景的描述符集相关
	DescriptorAllocator globalDescriptorAllocator;
	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;
	// 计算着色器绘制背景的管线布局
	VkPipelineLayout _gradientPipelineLayout;

	// 场景级别的信息：如mvp矩阵、光照参数等
	GPUSceneData _sceneData;
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

	// 绘制网格模型的管线布局
	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;
	// 包含单图片的描述符集布局
	VkDescriptorSetLayout _singleImageDescriptorLayout;

	// 场景默认数据
	std::vector<std::shared_ptr<MeshAsset>> _testMeshes;
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _grayImage;
	AllocatedImage _errorCheckerboardImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	// 用于上传cpu数据到gpu
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	// 窗口背景效果取决于使用哪个计算着色器
	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };
private:
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_default_data();
	void init_pipelines();
	void init_background_pipeline();
	void init_mesh_pipeline();

	void init_imgui();

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);

	void create_swapchain(uint32_t width, uint32_t height);
	void resize_swapchain();
	void destroy_swapchain();
};
