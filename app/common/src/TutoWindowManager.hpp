// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TUTO_WINDOW_MANAGER_HPP
#define TUTO_WINDOW_MANAGER_HPP

#include <vulkan_wrapper.h>
#include <stdexcept>
#include <android/native_window.h>

extern VkInstance tutorialInstance;
extern VkPhysicalDevice tutorialGpu;
extern VkDevice tutorialDevice;
extern VkQueue tutorialGraphicsQueue;
extern VkPhysicalDeviceMemoryProperties tutorialMemoryProperties;

extern VkSurfaceKHR tutorialSurface;
extern VkSwapchainKHR tutorialSwapchain;
extern VkExtent2D tutorialDisplaySize;
extern VkFormat tutorialDisplayFormat;
extern uint32_t tutorialSwapchainLength;

extern VkFramebuffer *tutorialFramebuffer;

void tutorialInitWindow(ANativeWindow *platformWindow,
                        VkApplicationInfo *appInfo);
void tutorialCreateSwapChain();
void tutorialCreateFrameBuffers(VkRenderPass &renderPass,
                                VkImageView depthView = VK_NULL_HANDLE);
void tutorialCleanup();

#endif  // TUTO_WINDOW_MANAGER_HPP
