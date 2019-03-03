#include "ValidationLayers.h"
#include <android/log.h>
#include <assert.h>
#include "Util.h"

// Simple Dbg Callback function to be used by Vk engine
static VkBool32 VKAPI_PTR DebugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                                              uint64_t object, size_t location, int32_t messageCode,
                                              const char* pLayerPrefix, const char* pMessage, void* pUserData) {
    if ((flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0) {
        LOGI("%s -- %s", pLayerPrefix, pMessage);
    }
    if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0) {
        LOGW("%s -- %s", pLayerPrefix, pMessage);
    }
    if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0) {
        LOGW("(PERFORMANCE) %s -- %s", pLayerPrefix, pMessage);
    }
    if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0) {
        LOGE("%s -- %s", pLayerPrefix, pMessage);
    }
    if ((flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0) {
        LOGD("%s -- %s", pLayerPrefix, pMessage);
    }

    return VK_FALSE;
}

void CreateDebugReportExt(VkInstance instance, VkDebugReportCallbackEXT* debugCallbackHandle) {
    if (vkCreateDebugReportCallbackEXT == nullptr) {
        vkCreateDebugReportCallbackEXT =
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
        vkDestroyDebugReportCallbackEXT =
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        vkDebugReportMessageEXT =
            (PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(instance, "vkDebugReportMessageEXT");
    }

    VkDebugReportCallbackCreateInfoEXT dbgInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                 VK_DEBUG_REPORT_ERROR_BIT_EXT,
        .pfnCallback = DebugReportCallback,
        .pUserData = nullptr,
    };
    CALL_VK(vkCreateDebugReportCallbackEXT(instance, &dbgInfo, nullptr, debugCallbackHandle));
}

void DestroyDebugReportExt(VkInstance instance, VkDebugReportCallbackEXT debugCallbackHandle) {
    vkDestroyDebugReportCallbackEXT(instance, debugCallbackHandle, nullptr);
}
