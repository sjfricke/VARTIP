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

#ifndef TUTORIAL_TEXTURES_HPP
#define TUTORIAL_TEXTURES_HPP

#include <android/asset_manager.h>
#include <vulkan_wrapper.h>

typedef struct texture_object {
  VkSampler sampler;
  VkImage image;
  VkImageLayout imageLayout;
  VkDeviceMemory mem;
  VkImageView view;
  int32_t tex_width, tex_height;
} texture_object;

VkResult tutorialLoadTextureFromFile(const char* filePath,
                                     struct texture_object* tex_obj,
                                     VkImageUsageFlags usage,
                                     VkFlags required_props);

static const VkFormat kTexFmt = VK_FORMAT_R8G8B8A8_UNORM;

#endif  // TUTORIAL_TEXTURES_HPP