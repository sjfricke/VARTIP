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

#include "TutorialUtils.hpp"
#include "TutorialTextures.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#include "TutoWindowManager.hpp"
#include "TutorialUtils.hpp"

#include <stdexcept>

extern VkDevice tutorialDevice;
extern AAssetManager* tutorialAssetManager;
extern VkPhysicalDeviceMemoryProperties tutorialMemoryProperties;
extern VkCommandPool cmdPool;
extern VkPhysicalDevice tutorialGpu;

// Open texture file from asset, load it into the created texture
// The supported texture format is in kTexFmt
//     Skipping memory barriers the next draw command is far out
//     by then, the blit will way complete ahead
VkResult tutorialLoadTextureFromFile(const char* filePath,
                                     struct texture_object* tex_obj,
                                     VkImageUsageFlags usage,
                                     VkFlags required_props) {
  if (!(usage | required_props)) {
    __android_log_print(ANDROID_LOG_ERROR,
                        "tutorial texture", "No usage and required_pros");
    return VK_ERROR_FORMAT_NOT_SUPPORTED;
  }

  // Check for linear supportability
  VkFormatProperties props;
  bool  needBlit = true;
  vkGetPhysicalDeviceFormatProperties(tutorialGpu, kTexFmt, &props);
  assert((props.linearTilingFeatures | props.optimalTilingFeatures) &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

  if (props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
    // linear format supporting the required texture
    needBlit = false;
  }

    // Read the file:
  AAsset* file = AAssetManager_open(tutorialAssetManager, filePath,
                                    AASSET_MODE_BUFFER);
  size_t fileLength = AAsset_getLength(file);
  stbi_uc* fileContent = new unsigned char[fileLength];
  AAsset_read(file, fileContent, fileLength);

  uint32_t imgWidth, imgHeight, n;
  unsigned char* imageData = stbi_load_from_memory(
          fileContent, fileLength, reinterpret_cast<int*>(&imgWidth),
          reinterpret_cast<int*>(&imgHeight), reinterpret_cast<int*>(&n), 0);

  tex_obj->tex_width = imgWidth;
  tex_obj->tex_height = imgHeight;

  // Allocate the linear texture so texture could be copied over
  VkImageCreateInfo image_create_info = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          .pNext = nullptr,
          .imageType = VK_IMAGE_TYPE_2D,
          .format = kTexFmt,
          .extent = {static_cast<uint32_t>(imgWidth),
                     static_cast<uint32_t>(imgHeight), 1},
          .mipLevels = 1,
          .arrayLayers = 1,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .tiling = VK_IMAGE_TILING_LINEAR,
          .usage = (needBlit ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT :
                               VK_IMAGE_USAGE_SAMPLED_BIT),
          .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
          .queueFamilyIndexCount = 0,
          .flags = 0,
  };
  VkMemoryAllocateInfo mem_alloc = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
          .pNext = nullptr,
          .allocationSize = 0,
          .memoryTypeIndex = 0,
  };

  VkMemoryRequirements mem_reqs;
  CALL_VK(vkCreateImage(tutorialDevice, &image_create_info,
                      nullptr, &tex_obj->image));
  vkGetImageMemoryRequirements(tutorialDevice, tex_obj->image, &mem_reqs);
  mem_alloc.allocationSize = mem_reqs.size;
  VK_CHECK(memory_type_from_properties(mem_reqs.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                              &mem_alloc.memoryTypeIndex));
  CALL_VK(vkAllocateMemory(tutorialDevice, &mem_alloc, nullptr, &tex_obj->mem));
  CALL_VK(vkBindImageMemory(tutorialDevice, tex_obj->image, tex_obj->mem, 0));

  if (required_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    const VkImageSubresource subres = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .arrayLayer = 0,
    };
    VkSubresourceLayout layout;
    void* data;

    vkGetImageSubresourceLayout(tutorialDevice, tex_obj->image, &subres,
                                &layout);
    CALL_VK(vkMapMemory(tutorialDevice, tex_obj->mem, 0, mem_alloc.allocationSize,
                      0, &data));

    for (int32_t y = 0; y < imgHeight; y++) {
      unsigned char* row = (unsigned char*)((char*)data + layout.rowPitch * y);
      for (int32_t x = 0; x < imgWidth; x++) {
        row[x * 4] = imageData[(x + y * imgWidth) * 4];
        row[x * 4 + 1] = imageData[(x + y * imgWidth) * 4 + 1];
        row[x * 4 + 2] = imageData[(x + y * imgWidth) * 4 + 2];
        row[x * 4 + 3] = imageData[(x + y * imgWidth) * 4 + 3];
      }
    }

    vkUnmapMemory(tutorialDevice, tex_obj->mem);
    delete[] imageData;
  }
  delete [] fileContent;

  tex_obj->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
 
  // If linear is supported, we are done
  if (!needBlit) {
    return VK_SUCCESS;
  }

  // save current image and mem as staging image and memory
  VkImage stageImage = tex_obj->image;
  VkDeviceMemory stageMem = tex_obj->mem;
  tex_obj->image = VK_NULL_HANDLE;
  tex_obj->mem   = VK_NULL_HANDLE;

  // Create a tile texture to blit into
  image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_create_info.usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT;
  CALL_VK(vkCreateImage(tutorialDevice, &image_create_info,
                                    nullptr, &tex_obj->image));
  vkGetImageMemoryRequirements(tutorialDevice, tex_obj->image, &mem_reqs);

  mem_alloc.allocationSize = mem_reqs.size;
  VK_CHECK(memory_type_from_properties(mem_reqs.memoryTypeBits,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     &mem_alloc.memoryTypeIndex));
  CALL_VK(vkAllocateMemory(tutorialDevice, &mem_alloc, nullptr, &tex_obj->mem));
  CALL_VK(vkBindImageMemory(tutorialDevice, tex_obj->image, tex_obj->mem, 0));

  VkCommandBuffer gfxCmd;
  const VkCommandBufferAllocateInfo cmd = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .pNext = nullptr,
          .commandPool = cmdPool,
          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          .commandBufferCount = 1,
  };

  CALL_VK(vkAllocateCommandBuffers(tutorialDevice, &cmd, &gfxCmd));

  VkCommandBufferBeginInfo cmd_buf_info = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .pNext = nullptr,
          .flags = 0,
          .pInheritanceInfo = nullptr};
  CALL_VK(vkBeginCommandBuffer(gfxCmd, &cmd_buf_info));

  VkImageCopy bltInfo = {
    .srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .srcSubresource.mipLevel = 0,
    .srcSubresource.baseArrayLayer = 0,
    .srcSubresource.layerCount = 1,
    .srcOffset.x = 0,
    .srcOffset.y = 0,
    .srcOffset.z = 0,
    .dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    .dstSubresource.mipLevel = 0,
    .dstSubresource.baseArrayLayer = 0,
    .dstSubresource.layerCount = 1,
    .dstOffset.x = 0,
    .dstOffset.y = 0,
    .dstOffset.z = 0,
    .extent.width = imgWidth,
    .extent.height = imgHeight,
    .extent.depth = 1,
  };
  vkCmdCopyImage(gfxCmd, stageImage,
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex_obj->image,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bltInfo);

  CALL_VK(vkEndCommandBuffer(gfxCmd));
  VkFenceCreateInfo fenceInfo = {
     .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
     .pNext = nullptr,
     .flags = 0,
  };
  VkFence  fence;
  CALL_VK(vkCreateFence(tutorialDevice, &fenceInfo, nullptr, &fence));

  VkSubmitInfo submitInfo = {
    .pNext = nullptr,
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 0,
    .pWaitSemaphores = nullptr,
    .pWaitDstStageMask = nullptr,
    .commandBufferCount = 1,
    .pCommandBuffers = &gfxCmd,
    .signalSemaphoreCount = 0,
    .pSignalSemaphores = nullptr,
  };
  CALL_VK(vkQueueSubmit(tutorialGraphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS);
  CALL_VK(vkWaitForFences(tutorialDevice, 1, &fence, VK_TRUE, 100000000) !=VK_SUCCESS);
  vkDestroyFence(tutorialDevice, fence, nullptr);

  vkFreeCommandBuffers(tutorialDevice, cmdPool, 1, &gfxCmd);
  vkDestroyImage(tutorialDevice, stageImage, nullptr);
  vkFreeMemory(tutorialDevice, stageMem, nullptr);
  return VK_SUCCESS;
}
