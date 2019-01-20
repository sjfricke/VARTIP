#ifndef VARTIP_VALIDATIONLAYERS_H_
#define VARTIP_VALIDATIONLAYERS_H_

#include <vulkan_wrapper.h>

void CreateDebugReportExt(VkInstance instance, VkDebugReportCallbackEXT* debugCallbackHandle);
void DestroyDebugReportExt(VkInstance instance, VkDebugReportCallbackEXT debugCallbackHandle);

#endif  // VARTIP_VALIDATIONLAYERS_H_
