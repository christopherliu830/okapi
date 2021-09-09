#include "pipeline.h"

namespace Graphics {
    PipelineBuilder::PipelineBuilder() { }

    vk::ResultValue<vk::Pipeline> PipelineBuilder::Build(vk::Device device, vk::RenderPass renderPass) {
        vk::GraphicsPipelineCreateInfo pipe {{}, _shaderStages};
        pipe.pVertexInputState = &_vertexInput;
        pipe.pInputAssemblyState = &_inputAssembly;
        pipe.pRasterizationState = &_rasterizer;
        pipe.pColorBlendState = &_blend;
        pipe.pMultisampleState = &_multisample;
        pipe.pViewportState = &_viewport;
        pipe.pDepthStencilState = &_depthStencil;
        pipe.pDynamicState = &_dynamic;
        pipe.renderPass = renderPass;
        pipe.layout = _layout;

        return device.createGraphicsPipeline(nullptr, pipe);
    }

    vk::ResultValue<vk::UniquePipeline> PipelineBuilder::BuildUnique(vk::Device device, vk::RenderPass renderPass) {
        vk::GraphicsPipelineCreateInfo pipe {{}, _shaderStages};
        pipe.pVertexInputState = &_vertexInput;
        pipe.pInputAssemblyState = &_inputAssembly;
        pipe.pRasterizationState = &_rasterizer;
        pipe.pColorBlendState = &_blend;
        pipe.pMultisampleState = &_multisample;
        pipe.pViewportState = &_viewport;
        pipe.pDepthStencilState = &_depthStencil;
        pipe.pDynamicState = &_dynamic;
        pipe.renderPass = renderPass;
        pipe.layout = _layout;

        return device.createGraphicsPipelineUnique(nullptr, pipe);
    }

    PipelineBuilder* PipelineBuilder::SetVertexInput(vk::PipelineVertexInputStateCreateInfo info) {
        _vertexInput = info;
        return this;
    }

    PipelineBuilder* PipelineBuilder::SetRasterizer(vk::PipelineRasterizationStateCreateInfo info) {
        _rasterizer = info;
        return this;
    }

    PipelineBuilder* PipelineBuilder::SetInputAssembly(vk::PipelineInputAssemblyStateCreateInfo info) {
        _inputAssembly = info;
        return this;
    }

    PipelineBuilder* PipelineBuilder::SetColorBlendState(vk::PipelineColorBlendStateCreateInfo info) {
        _blend = info;
        return this;
    }

    PipelineBuilder* PipelineBuilder::SetMultisample(vk::PipelineMultisampleStateCreateInfo info) {
        _multisample = info;
        return this;
    }

    PipelineBuilder* PipelineBuilder::SetDepthStencil(vk::PipelineDepthStencilStateCreateInfo info) {
        _depthStencil = info;
        return this;
    }

    PipelineBuilder* PipelineBuilder::SetDynamicState(vk::PipelineDynamicStateCreateInfo info) {
        _dynamic = info;
        return this;
    }

    PipelineBuilder* PipelineBuilder::SetViewport(vk::PipelineViewportStateCreateInfo info) {
        _viewport = info;
        return this;
    }

    PipelineBuilder* PipelineBuilder::SetPipelineLayout(vk::PipelineLayout layout) {
        _layout = layout;
        return this;
    }

    PipelineBuilder* PipelineBuilder::AddShaderModule(vk::PipelineShaderStageCreateInfo info) {
        _shaderStages.push_back(info);
        return this;
    }

    PipelineBuilder* PipelineBuilder::FlushShaderModules() {
        _shaderStages.clear();
        return this;
    }
}