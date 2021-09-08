#pragma once

#include "graphics.h"

namespace Graphics {


    class PipelineBuilder {
    public:
        PipelineBuilder();

        vk::ResultValue<vk::Pipeline> Build(vk::Device device, vk::RenderPass renderPass);
        PipelineBuilder* SetVertexInput(vk::PipelineVertexInputStateCreateInfo info);
        PipelineBuilder* SetRasterizer(vk::PipelineRasterizationStateCreateInfo info);
        PipelineBuilder* SetInputAssembly(vk::PipelineInputAssemblyStateCreateInfo info);
        PipelineBuilder* SetColorBlendState(vk::PipelineColorBlendStateCreateInfo info);
        PipelineBuilder* SetMultisample(vk::PipelineMultisampleStateCreateInfo info);
        PipelineBuilder* SetDepthStencil(vk::PipelineDepthStencilStateCreateInfo info);
        PipelineBuilder* SetDynamicState(vk::PipelineDynamicStateCreateInfo info);
        PipelineBuilder* SetViewport(vk::PipelineViewportStateCreateInfo info);
        PipelineBuilder* SetPipelineLayout(vk::PipelineLayout layout);
        PipelineBuilder* AddShaderModule(vk::PipelineShaderStageCreateInfo info);
        PipelineBuilder* FlushShaderModules();

    private:
        vk::PipelineVertexInputStateCreateInfo _vertexInput;
        vk::PipelineInputAssemblyStateCreateInfo _inputAssembly;
        vk::PipelineRasterizationStateCreateInfo _rasterizer;
        vk::PipelineColorBlendStateCreateInfo _blend;
        vk::PipelineMultisampleStateCreateInfo _multisample;
        vk::PipelineViewportStateCreateInfo _viewport;
        vk::PipelineDepthStencilStateCreateInfo _depthStencil;
        vk::PipelineDynamicStateCreateInfo _dynamic;
        vk::PipelineLayout _layout;
        std::vector<vk::PipelineShaderStageCreateInfo> _shaderStages;
    };




}