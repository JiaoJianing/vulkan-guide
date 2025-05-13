#include <vk_pipelines.h>
#include <fstream>
#include <vk_initializers.h>

namespace vkutil {
	bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
	{
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			return false;
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

		file.seekg(0);
		file.read((char*)buffer.data(), fileSize);
		file.close();

		VkShaderModuleCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.pNext = nullptr };
		createInfo.codeSize = buffer.size() * sizeof(uint32_t);
		createInfo.pCode = buffer.data();

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			return false;
		}

		*outShaderModule = shaderModule;
		return true;
	}

	void PipelineBuilder::clear()
	{
		_inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
		_rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
		_colorBlendAttachment = {};
		_multiSampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		_pipelineLayout = {};
		_depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
		_renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
		_shaderStages.clear();
	}

	void PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
	{
		_shaderStages.clear();
		_shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
		_shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
	}

	void PipelineBuilder::set_input_topology(VkPrimitiveTopology topology)
	{
		_inputAssembly.topology = topology;
		_inputAssembly.primitiveRestartEnable = VK_FALSE;
	}

	void PipelineBuilder::set_polygon_mode(VkPolygonMode mode)
	{
		_rasterizer.polygonMode = mode;
		_rasterizer.lineWidth = 1.0f;
	}

	void PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace)
	{
		_rasterizer.cullMode = cullMode;
		_rasterizer.frontFace = frontFace;
	}

	void PipelineBuilder::set_mulitSampling_none()
	{
		_multiSampling.sampleShadingEnable = VK_FALSE;
		_multiSampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		_multiSampling.minSampleShading = 1.0f;
		_multiSampling.pSampleMask = nullptr;
		_multiSampling.alphaToCoverageEnable = VK_FALSE;
		_multiSampling.alphaToOneEnable = VK_FALSE;
	}

	void PipelineBuilder::disable_blending()
	{
		_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		_colorBlendAttachment.blendEnable = VK_FALSE;
	}

	void PipelineBuilder::set_color_attachment_format(VkFormat format)
	{
		_colorAttachmentFormat = format;
		_renderInfo.colorAttachmentCount = 1;
		_renderInfo.pColorAttachmentFormats = &_colorAttachmentFormat;
	}

	void PipelineBuilder::set_depth_format(VkFormat format)
	{
		_renderInfo.depthAttachmentFormat = format;
	}

	void PipelineBuilder::disable_depthTest()
	{
		_depthStencil.depthTestEnable = VK_FALSE;
		_depthStencil.depthWriteEnable = VK_FALSE;
		_depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
		_depthStencil.depthBoundsTestEnable = VK_FALSE;
		_depthStencil.stencilTestEnable = VK_FALSE;
		_depthStencil.front = {};
		_depthStencil.back = {};
		_depthStencil.minDepthBounds = 0.0f;
		_depthStencil.maxDepthBounds = 1.0f;
	}

	VkPipeline PipelineBuilder::build_pipeline(VkDevice device)
	{
		VkPipelineViewportStateCreateInfo viewportState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .pNext = nullptr };
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkPipelineColorBlendStateCreateInfo colorBlending = { .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .pNext = nullptr };
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &_colorBlendAttachment;

		VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

		VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
		pipelineInfo.pNext = &_renderInfo;

		pipelineInfo.stageCount = (uint32_t)_shaderStages.size();
		pipelineInfo.pStages = _shaderStages.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &_inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &_rasterizer;
		pipelineInfo.pMultisampleState = &_multiSampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDepthStencilState = &_depthStencil;
		pipelineInfo.layout = _pipelineLayout;

		VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
		dynamicInfo.pDynamicStates = &state[0];
		dynamicInfo.dynamicStateCount = 2;
		pipelineInfo.pDynamicState = &dynamicInfo;

		VkPipeline newPipeline;
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS)
		{
			fmt::println("failed to create pipeline");
			return VK_NULL_HANDLE;
		}
		else
		{
			return newPipeline;
		}
	}

};
