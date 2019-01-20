#ifndef VARTIP_VULKANMAIN_H_
#define VARTIP_VULKANMAIN_H_

// Initialize vulkan device context
// after return, vulkan is ready to draw
#include <android_native_app_glue.h>
#include "ImageReader.h"
#include "NativeCamera.h"
#include "Util.h"
bool InitVulkan(android_app* app);

void InitCamera();

// delete vulkan device context when application goes away
void DeleteVulkan(void);

// Check if vulkan is ready to draw
bool IsVulkanReady(void);

// Ask Vulkan to Render a frame
bool VulkanDrawFrame(android_app* app);

#endif  // VARTIP_VULKANMAIN_H_
