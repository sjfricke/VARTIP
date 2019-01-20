#ifndef VARTIP_CREATESHADERMODULE_H_
#define VARTIP_CREATESHADERMODULE_H_

#include <android_native_app_glue.h>
#include <vulkan_wrapper.h>

VkShaderModule LoadSPIRVShader(android_app* appInfo, const char* filePath, VkDevice vkDevice);

#endif  // VARTIP_CREATESHADERMODULE_H_
