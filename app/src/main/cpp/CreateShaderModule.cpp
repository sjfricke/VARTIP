#include "CreateShaderModule.h"
#include <android/log.h>
#include <assert.h>
#include "Util.h"

VkShaderModule LoadSPIRVShader(android_app* appInfo, const char* filePath, VkDevice vkDevice) {
    // Shaders need to compiled prior
    AAsset* asset = AAssetManager_open(appInfo->activity->assetManager, filePath, AASSET_MODE_BUFFER);
    ASSERT(asset != nullptr, "Make sure you have ran the build_shader.py script prior to compiling!");
    size_t shaderSize = AAsset_getLength(asset);
    assert(shaderSize > 0);

    char* shaderCode = new char[shaderSize];
    AAsset_read(asset, shaderCode, shaderSize);
    AAsset_close(asset);
    assert(shaderCode != nullptr);

    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = shaderSize;
    shaderModuleCreateInfo.pCode = (uint32_t*)shaderCode;

    VkShaderModule shaderModule;
    CALL_VK(vkCreateShaderModule(vkDevice, &shaderModuleCreateInfo, nullptr, &shaderModule));

    delete[] shaderCode;
    return shaderModule;
}
