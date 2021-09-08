#include <SDL2/sdl.h>
#include <SDL2/SDL_vulkan.h>
#include <iostream>
#include "screen.h"

VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}


Screen::Screen() {
    if (SDL_Init( SDL_INIT_EVERYTHING ) < 0) {
        fprintf(stderr, "Could not initialize sdl2: %s\n", SDL_GetError());
    }

    window = SDL_CreateWindow(
        "Space Crawler 0.1.0",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN
    );

    if (window == NULL) { fprintf(stderr, "Could not create window: %s\n", SDL_GetError()); }

    vulkanInit();
}


Screen::~Screen() {

    if (gEnableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Screen::update() {
    return;
}

void Screen::vulkanInit() {

    if (gEnableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    } 

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Space Crawler";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t extensionCount;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr)) {
        fprintf(stderr, "%s\n", SDL_GetError());
    };
    std::vector<const char *> extensions; 

    if (gEnableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    size_t additionalExtensionCount = extensions.size();
    extensions.resize(extensionCount + additionalExtensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data() + additionalExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (gEnableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(gValidationLayers.size());
        createInfo.ppEnabledLayerNames = gValidationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

    if (gEnableValidationLayers) {
        VkDebugUtilsMessengerCreateInfoEXT m;
        m.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        m.messageSeverity = (
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT 
        );
        m.messageType = (
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
        );
        m.pfnUserCallback = vkDebugCallback;
        m.pNext = NULL;
        m.flags = 0;

        if (CreateDebugUtilsMessengerEXT(instance, &m, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to set up vk debug messenger");
        }
    }
}

bool Screen::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for(const char* layerName : gValidationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) return false;
    }

    return true;
}
