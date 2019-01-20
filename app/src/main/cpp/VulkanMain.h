#ifndef VARTIP_VULKANMAIN_H_
#define VARTIP_VULKANMAIN_H_

#include <android_native_app_glue.h>
#include "ImageReader.h"
#include "NativeCamera.h"
#include "Util.h"

#define VARTIP_VALIDATION_LAYERS true

bool InitVulkan(android_app* app);

void InitCamera();

void DeleteVulkan(void);

bool IsVulkanReady(void);

bool VulkanDrawFrame(android_app* app);

#endif  // VARTIP_VULKANMAIN_H_
