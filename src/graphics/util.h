#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>

namespace Graphics {
    std::vector<char> ReadFile(const std::string& filename) {
        // Start at end of file to get size of buffer later
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);
        
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return buffer;
    }

    /**
     * @brief Validates a list of required layers, comparing it with the available ones.
     *
     * @param required A vector containing required layer names.
     * @param available A VkLayerProperties object containing available layers.
     * @return true if all required extensions are available
     * @return false otherwise
     */
    bool AreRequiredExtensionsPresent(
        std::vector<const char *> required,
        std::vector<vk::ExtensionProperties> &available
    ) {

        for(auto extension : required) {
            bool extensionFound = false;
            for (const auto& availableExtension : available) {
                if (strcmp(extension, availableExtension.extensionName) == 0) {
                    extensionFound = true;
                    break;
                }
            }
            if (!extensionFound) return false;
        }
        return true;
    }

    bool AreRequiredValidationLayersPresent(
        std::vector<const char *> required,
        std::vector<vk::LayerProperties> &available
    ) {

        for(auto layer : required) {
            bool layerFound = false;
            for (const auto& availableLayer : available) {
                std::cout << availableLayer.layerName << std::endl;
                if (strcmp(layer, availableLayer.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) return false;
        }
        return true;
    }

    // https://github.com/KhronosGroup/Vulkan-Hpp/blob/master/samples/utils/utils.cpp
    VkBool32 DebugUtilsMessengerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
        VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData,
    void * /*pUserData*/ ) {
    #ifndef NDEBUG
        if ( pCallbackData->messageIdNumber == 648835635 )
        {
            // UNASSIGNED-khronos-Validation-debug-build-warning-message
            return VK_FALSE;
        }
        if ( pCallbackData->messageIdNumber == 767975156 )
        {
            // UNASSIGNED-BestPractices-vkCreateInstance-specialuse-extension
            return VK_FALSE;
        }
    #endif

        std::cerr << vk::to_string( static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>( messageSeverity ) ) << ": "
                    << vk::to_string( static_cast<vk::DebugUtilsMessageTypeFlagsEXT>( messageTypes ) ) << ":\n";
        std::cerr << "\t"
                    << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
        std::cerr << "\t"
                    << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
        std::cerr << "\t"
                    << "message         = <" << pCallbackData->pMessage << ">\n";
        if ( 0 < pCallbackData->queueLabelCount )
        {
            std::cerr << "\t"
                    << "Queue Labels:\n";
            for ( uint8_t i = 0; i < pCallbackData->queueLabelCount; i++ )
            {
            std::cerr << "\t\t"
                        << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
            }
        }
        if ( 0 < pCallbackData->cmdBufLabelCount )
        {
            std::cerr << "\t"
                    << "CommandBuffer Labels:\n";
            for ( uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++ )
            {
            std::cerr << "\t\t"
                        << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
            }
        }
        if ( 0 < pCallbackData->objectCount )
        {
            std::cerr << "\t"
                    << "Objects:\n";
            for ( uint8_t i = 0; i < pCallbackData->objectCount; i++ )
            {
            std::cerr << "\t\t"
                        << "Object " << i << "\n";
            std::cerr << "\t\t\t"
                        << "objectType   = "
                        << vk::to_string( static_cast<vk::ObjectType>( pCallbackData->pObjects[i].objectType ) ) << "\n";
            std::cerr << "\t\t\t"
                        << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
            if ( pCallbackData->pObjects[i].pObjectName )
            {
                std::cerr << "\t\t\t"
                        << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
            }
            }
        }
        return VK_TRUE;
    }
}
