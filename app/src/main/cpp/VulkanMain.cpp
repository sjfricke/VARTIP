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

#include <vector>
#include <cassert>
#include <android/log.h>
#include <malloc.h>
#include "vulkan_wrapper.h"
#include "VulkanMain.hpp"
#include "CreateShaderModule.h"

// Vulkan call wrapper
#define CALL_VK(func)                                                 \
  if (VK_SUCCESS != (func)) {                                         \
    __android_log_print(ANDROID_LOG_ERROR, "Tutorial ",               \
                        "Vulkan error. File[%s], line[%d]", __FILE__, \
                        __LINE__);                                    \
    assert(false);                                                    \
  }

// A macro to check value is VK_SUCCESS
// Used also for non-vulkan functions but return VK_SUCCESS
#define VK_CHECK(x)  CALL_VK(x)

// Global Variables ...
struct VulkanDeviceInfo {
    bool initialized_;

    VkInstance          instance_;
    VkPhysicalDevice    gpuDevice_;
    VkPhysicalDeviceMemoryProperties gpuMemoryProperties_;
    VkDevice            device_;

    VkSurfaceKHR        surface_;
    VkQueue             queue_;
};
VulkanDeviceInfo  device;

struct VulkanSwapchainInfo {
    VkSwapchainKHR swapchain_;
    uint32_t swapchainLength_;

    VkExtent2D displaySize_;
    VkFormat displayFormat_;

    // array of frame buffers and views
    VkFramebuffer* framebuffers_;
    VkImageView* displayViews_;
};
VulkanSwapchainInfo  swapchain;

typedef struct texture_object {
    VkSampler sampler;
    VkImage image;
    VkImageLayout imageLayout;
    VkDeviceMemory mem;
    VkImageView view;
    int32_t tex_width, tex_height;
} texture_object;
static const VkFormat kTexFmt = VK_FORMAT_R8G8B8A8_UNORM;
#define TUTORIAL_TEXTURE_COUNT 1
struct texture_object textures[TUTORIAL_TEXTURE_COUNT];

struct VulkanBufferInfo {
    VkBuffer vertexBuf;
};
VulkanBufferInfo buffers;

struct VulkanGfxPipelineInfo {
    VkDescriptorSetLayout dscLayout;
    VkDescriptorPool  descPool;
    VkDescriptorSet   descSet;
    VkPipelineLayout  layout;
    VkPipelineCache   cache;
    VkPipeline        pipeline;
};
VulkanGfxPipelineInfo gfxPipeline;

struct VulkanRenderInfo {
    VkRenderPass renderPass_;
    VkCommandPool cmdPool_;
    VkCommandBuffer* cmdBuffer_;
    uint32_t         cmdBufferLen_;
    VkSemaphore   semaphore_;
    VkFence       fence_;
};
VulkanRenderInfo render;

// Camera variables
NativeCamera* m_native_camera;

camera_type m_selected_camera_type = BACK_CAMERA; // Default

// Image Reader
ImageFormat m_view{0, 0, 0};
ImageReader* m_image_reader;
AImage* m_image;

volatile bool m_camera_ready;

// Android Native App pointer...
android_app* androidAppCtx = nullptr;

// Create vulkan device
void CreateVulkanDevice(ANativeWindow* platformWindow,
                        VkApplicationInfo* appInfo) {
  std::vector<const char *> instance_extensions;
  std::vector<const char *> device_extensions;

  instance_extensions.push_back("VK_KHR_surface");
  instance_extensions.push_back("VK_KHR_android_surface");

  device_extensions.push_back("VK_KHR_swapchain");

  // **********************************************************
  // Create the Vulkan instance
  VkInstanceCreateInfo instanceCreateInfo {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .pApplicationInfo = appInfo,
      .enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size()),
      .ppEnabledExtensionNames = instance_extensions.data(),
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
  };
  CALL_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &device.instance_));
  VkAndroidSurfaceCreateInfoKHR createInfo {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .window = platformWindow};

  CALL_VK(vkCreateAndroidSurfaceKHR(device.instance_, &createInfo, nullptr,
                                    &device.surface_));
  // Find one GPU to use:
  // On Android, every GPU device is equal -- supporting graphics/compute/present
  // for this sample, we use the very first GPU device found on the system
  uint32_t  gpuCount = 0;
  CALL_VK(vkEnumeratePhysicalDevices(device.instance_, &gpuCount, nullptr));
  VkPhysicalDevice tmpGpus[gpuCount];
  CALL_VK(vkEnumeratePhysicalDevices(device.instance_, &gpuCount, tmpGpus));
  device.gpuDevice_ = tmpGpus[0];     // Pick up the first GPU Device

  vkGetPhysicalDeviceMemoryProperties(device.gpuDevice_,
                                      &device.gpuMemoryProperties_);

  // Create a logical device (vulkan device)
  float priorities[] = { 1.0f, };
  VkDeviceQueueCreateInfo queueCreateInfo {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueCount = 1,
      .queueFamilyIndex = 0,
      .pQueuePriorities = priorities,
  };

  VkDeviceCreateInfo deviceCreateInfo {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = nullptr,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueCreateInfo,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
      .ppEnabledExtensionNames = device_extensions.data(),
      .pEnabledFeatures = nullptr,
  };

  CALL_VK(vkCreateDevice(device.gpuDevice_, &deviceCreateInfo, nullptr,
                         &device.device_));
  vkGetDeviceQueue(device.device_, 0, 0, &device.queue_);
}

void CreateSwapChain() {
  LOGI("->createSwapChain");
  memset(&swapchain, 0, sizeof(swapchain));

  // **********************************************************
  // Get the surface capabilities because:
  //   - It contains the minimal and max length of the chain, we will need it
  //   - It's necessary to query the supported surface format (R8G8B8A8 for
  //   instance ...)
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.gpuDevice_, device.surface_,
                                            &surfaceCapabilities);
  // Query the list of supported surface format and choose one we like
  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice_, device.surface_,
                                       &formatCount, nullptr);
  VkSurfaceFormatKHR *formats = new VkSurfaceFormatKHR [formatCount];
  vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice_, device.surface_,
                                       &formatCount, formats);
  LOGI("Got %d formats", formatCount);

  uint32_t chosenFormat;
  for (chosenFormat = 0; chosenFormat < formatCount; chosenFormat++) {
    if (formats[chosenFormat].format == VK_FORMAT_R8G8B8A8_UNORM)
      break;
  }
  assert(chosenFormat < formatCount);

  swapchain.displaySize_ = surfaceCapabilities.currentExtent;
  swapchain.displayFormat_ = formats[chosenFormat].format;

  // **********************************************************
  // Create a swap chain (here we choose the minimum available number of surface
  // in the chain)
  uint32_t queueFamily = 0;
  VkSwapchainCreateInfoKHR swapchainCreateInfo {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .surface = device.surface_,
      .minImageCount = surfaceCapabilities.minImageCount,
      .imageFormat = formats[chosenFormat].format,
      .imageColorSpace = formats[chosenFormat].colorSpace,
      .imageExtent = surfaceCapabilities.currentExtent,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .imageArrayLayers = 1,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamily,
      .presentMode = VK_PRESENT_MODE_FIFO_KHR,
      .oldSwapchain = VK_NULL_HANDLE,
      .clipped = VK_FALSE,
  };
  CALL_VK(vkCreateSwapchainKHR(device.device_, &swapchainCreateInfo,
                               nullptr, &swapchain.swapchain_));

  // Get the length of the created swap chain
  CALL_VK(vkGetSwapchainImagesKHR(device.device_, swapchain.swapchain_,
                                  &swapchain.swapchainLength_, nullptr));
  delete [] formats;
  LOGI("<-createSwapChain");
}

void CreateFrameBuffers(VkRenderPass& renderPass,
                        VkImageView depthView = VK_NULL_HANDLE) {

  // query display attachment to swapchain
  uint32_t SwapchainImagesCount = 0;
  CALL_VK(vkGetSwapchainImagesKHR(device.device_, swapchain.swapchain_,
                                  &SwapchainImagesCount, nullptr));
  VkImage* displayImages = new VkImage[SwapchainImagesCount];
  CALL_VK(vkGetSwapchainImagesKHR(device.device_, swapchain.swapchain_,
                                  &SwapchainImagesCount, displayImages));

  // create image view for each swapchain image
  swapchain.displayViews_ = new VkImageView[SwapchainImagesCount];
  for (uint32_t i = 0; i < SwapchainImagesCount; i++) {
    VkImageViewCreateInfo viewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = displayImages[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain.displayFormat_,
        .components = {
           .r = VK_COMPONENT_SWIZZLE_R,
           .g = VK_COMPONENT_SWIZZLE_G,
           .b = VK_COMPONENT_SWIZZLE_B,
           .a = VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .flags = 0,
    };
    CALL_VK(vkCreateImageView(device.device_, &viewCreateInfo, nullptr,
                              &swapchain.displayViews_[i]));
  }
  delete[] displayImages;

  // create a framebuffer from each swapchain image
  swapchain.framebuffers_ = new VkFramebuffer[swapchain.swapchainLength_];
  for (uint32_t i = 0; i < swapchain.swapchainLength_; i++) {
    VkImageView attachments[2] = {
            swapchain.displayViews_[i], depthView,
    };
    VkFramebufferCreateInfo fbCreateInfo {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .layers = 1,
        .attachmentCount = 1,  // 2 if using depth
        .pAttachments = attachments,
        .width = static_cast<uint32_t>(swapchain.displaySize_.width),
        .height = static_cast<uint32_t>(swapchain.displaySize_.height),
    };
    fbCreateInfo.attachmentCount = (depthView == VK_NULL_HANDLE ? 1 : 2);

    CALL_VK(vkCreateFramebuffer(device.device_, &fbCreateInfo, nullptr,
                                &swapchain.framebuffers_[i]));
  }
}

// A help function to map required memory property into a VK memory type
// memory type is an index into the array of 32 entries; or the bit index
// for the memory type ( each BIT of an 32 bit integer is a type ).
VkResult AllocateMemoryTypeFromProperties(
    uint32_t typeBits,
    VkFlags requirements_mask,
    uint32_t *typeIndex) {

  // Search memtypes to find first index with those properties
  for (uint32_t i = 0; i < 32; i++) {
    if ((typeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((device.gpuMemoryProperties_.memoryTypes[i].propertyFlags &
           requirements_mask) == requirements_mask) {
        *typeIndex = i;
        return VK_SUCCESS;
      }
    }
    typeBits >>= 1;
  }
  // No memory types matched, return failure
  return VK_ERROR_MEMORY_MAP_FAILED;
}

VkResult LoadTextureFromCamera(struct texture_object* tex_obj,
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
  vkGetPhysicalDeviceFormatProperties(device.gpuDevice_, kTexFmt, &props);
  assert((props.linearTilingFeatures | props.optimalTilingFeatures) &
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

  if (props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
    // linear format supporting the required texture
    needBlit = false;
  }

  uint32_t imgWidth = 480;
  uint32_t imgHeight = 720;
  uint32_t n = 4;

  unsigned char* imageData = (unsigned char*)malloc(n * imgHeight * imgHeight);

  uint32_t *imageDataPix = reinterpret_cast<uint32_t *>(imageData);

  // test make half blue and half red
  for (int ii = 0; ii < imgHeight; ii++) {
    for (int jj = 0; jj < imgWidth; jj++) {
      if (ii > 3* imgHeight/4) {
        imageDataPix[imgWidth * ii + jj ] = 0xFF0000FF;
      }
      else if (ii > imgHeight/2) {
        imageDataPix[imgWidth * ii + jj ] = 0xFF00FF00;
      }
      else
      if (ii > imgHeight/4) {
        imageDataPix[imgWidth * ii + jj ] = 0xFFFF0000;
      }
      else {
        imageDataPix[imgWidth * ii + jj ] = 0xFFFFFF00;
      }
    }
  }

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
  CALL_VK(vkCreateImage(device.device_, &image_create_info,
                        nullptr, &tex_obj->image));
  vkGetImageMemoryRequirements(device.device_, tex_obj->image, &mem_reqs);
  mem_alloc.allocationSize = mem_reqs.size;
  VK_CHECK(AllocateMemoryTypeFromProperties(mem_reqs.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                       &mem_alloc.memoryTypeIndex));
  CALL_VK(vkAllocateMemory(device.device_, &mem_alloc, nullptr, &tex_obj->mem));
  CALL_VK(vkBindImageMemory(device.device_, tex_obj->image, tex_obj->mem, 0));

  if (required_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    const VkImageSubresource subres = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .arrayLayer = 0,
    };
    VkSubresourceLayout layout;
    void* data;

    vkGetImageSubresourceLayout(device.device_, tex_obj->image, &subres,
                                &layout);
    CALL_VK(vkMapMemory(device.device_, tex_obj->mem, 0, mem_alloc.allocationSize,
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

    vkUnmapMemory(device.device_, tex_obj->mem);
    free(imageData);
   // stbi_image_free(imageData);
  }
 // delete [] fileContent;

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
  CALL_VK(vkCreateImage(device.device_, &image_create_info,
                        nullptr, &tex_obj->image));
  vkGetImageMemoryRequirements(device.device_, tex_obj->image, &mem_reqs);

  mem_alloc.allocationSize = mem_reqs.size;
  VK_CHECK(AllocateMemoryTypeFromProperties(mem_reqs.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       &mem_alloc.memoryTypeIndex));
  CALL_VK(vkAllocateMemory(device.device_, &mem_alloc, nullptr, &tex_obj->mem));
  CALL_VK(vkBindImageMemory(device.device_, tex_obj->image, tex_obj->mem, 0));

  VkCommandPoolCreateInfo cmdPoolCreateInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = 0,
  };

  VkCommandPool cmdPool;
  CALL_VK(vkCreateCommandPool(device.device_, &cmdPoolCreateInfo,
                              nullptr, &cmdPool));

  VkCommandBuffer gfxCmd;
  const VkCommandBufferAllocateInfo cmd = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = cmdPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  CALL_VK(vkAllocateCommandBuffers(device.device_, &cmd, &gfxCmd));
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
  CALL_VK(vkCreateFence(device.device_, &fenceInfo, nullptr, &fence));

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
  CALL_VK(vkQueueSubmit(device.queue_, 1, &submitInfo, fence) != VK_SUCCESS);
  CALL_VK(vkWaitForFences(device.device_, 1, &fence, VK_TRUE, 100000000) !=VK_SUCCESS);
  vkDestroyFence(device.device_, fence, nullptr);

  vkFreeCommandBuffers(device.device_, cmdPool, 1, &gfxCmd);
  vkDestroyCommandPool(device.device_, cmdPool, nullptr);
  vkDestroyImage(device.device_, stageImage, nullptr);
  vkFreeMemory(device.device_, stageMem, nullptr);
  return VK_SUCCESS;
}

void CreateTexture() {
  for (uint32_t i = 0; i < TUTORIAL_TEXTURE_COUNT; i++) {
    LoadTextureFromCamera(&textures[i],
        VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    const VkSamplerCreateInfo sampler = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .maxAnisotropy = 1,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE,
    };
    VkImageViewCreateInfo view = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = VK_NULL_HANDLE,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = kTexFmt,
        .components = {
            VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A,
        },
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        .flags = 0,
    };

    CALL_VK(vkCreateSampler(device.device_, &sampler,
            nullptr, &textures[i].sampler));
    view.image = textures[i].image;
    CALL_VK(vkCreateImageView(device.device_, &view, nullptr, &textures[i].view));
  }
}

// A helper function
bool MapMemoryTypeToIndex(uint32_t typeBits,
                              VkFlags requirements_mask,
                              uint32_t *typeIndex) {
  VkPhysicalDeviceMemoryProperties memoryProperties;
  vkGetPhysicalDeviceMemoryProperties(device.gpuDevice_, &memoryProperties);
  // Search memtypes to find first index with those properties
  for (uint32_t i = 0; i < 32; i++) {
    if ((typeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((memoryProperties.memoryTypes[i].propertyFlags &
           requirements_mask) == requirements_mask) {
        *typeIndex = i;
        return true;
      }
    }
    typeBits >>= 1;
  }
  return false;
}

// Create our vertex buffer
bool CreateBuffers(void) {
  // Vertex positions
  const float vertexData[] = {
          -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
          1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
          -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
          1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
          1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
          -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
  };

  // Create a vertex buffer
  uint32_t queueIdx = 0;
  VkBufferCreateInfo createBufferInfo {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .size = sizeof(vertexData),
      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .flags = 0,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .pQueueFamilyIndices = &queueIdx,
      .queueFamilyIndexCount = 1,
  };

  CALL_VK(vkCreateBuffer(device.device_, &createBufferInfo, nullptr,
                         &buffers.vertexBuf));

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(device.device_, buffers.vertexBuf, &memReq);

  VkMemoryAllocateInfo allocInfo {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = sizeof(vertexData),
      .memoryTypeIndex = 0,  // Memory type assigned in the next step
  };

  // Assign the proper memory type for that buffer
  allocInfo.allocationSize = memReq.size;
  MapMemoryTypeToIndex(memReq.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                       &allocInfo.memoryTypeIndex);

  // Allocate memory for the buffer
  VkDeviceMemory deviceMemory;
  CALL_VK(vkAllocateMemory(device.device_, &allocInfo, nullptr, &deviceMemory));

  void* data;
  CALL_VK(vkMapMemory(device.device_, deviceMemory, 0, sizeof(vertexData), 0,
                      &data));
  memcpy(data, vertexData, sizeof(vertexData));
  vkUnmapMemory(device.device_, deviceMemory);

  CALL_VK(vkBindBufferMemory(device.device_, buffers.vertexBuf, deviceMemory, 0));
  return true;
}

void DeleteBuffers(void) {
        vkDestroyBuffer(device.device_, buffers.vertexBuf, nullptr);
}

// Create Graphics Pipeline
VkResult CreateGraphicsPipeline() {
  memset(&gfxPipeline, 0, sizeof(gfxPipeline));

  const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = TUTORIAL_TEXTURE_COUNT,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr,
  };
  const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .bindingCount = 1,
      .pBindings = &descriptorSetLayoutBinding,
  };
  CALL_VK(vkCreateDescriptorSetLayout(device.device_,
          &descriptorSetLayoutCreateInfo, nullptr, &gfxPipeline.dscLayout));
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .setLayoutCount = 1,
      .pSetLayouts = &gfxPipeline.dscLayout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };
  CALL_VK(vkCreatePipelineLayout(device.device_, &pipelineLayoutCreateInfo,
                                 nullptr, &gfxPipeline.layout));

  // No dynamic state in that tutorial
  VkPipelineDynamicStateCreateInfo dynamicStateInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = nullptr,
      .dynamicStateCount = 0,
      .pDynamicStates = nullptr };

  VkShaderModule vertexShader,fragmentShader;
  buildShaderFromFile(androidAppCtx, "shaders/tri.vert",
                      VK_SHADER_STAGE_VERTEX_BIT,
                      device.device_, &vertexShader);
  buildShaderFromFile(androidAppCtx, "shaders/tri.frag",
                      VK_SHADER_STAGE_FRAGMENT_BIT,
                      device.device_, &fragmentShader);
  // Specify vertex and fragment shader stages
  VkPipelineShaderStageCreateInfo shaderStages[2] {
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vertexShader,
          .pSpecializationInfo = nullptr,
          .flags = 0,
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = fragmentShader,
          .pSpecializationInfo = nullptr,
          .flags = 0,
          .pName = "main",
      }
  };

  VkViewport viewports {
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
      .x = 0,
      .y = 0,
      .width = (float)swapchain.displaySize_.width,
      .height = (float)swapchain.displaySize_.height,
  };

  VkRect2D scissor = {
      .extent = swapchain.displaySize_,
      .offset = {.x = 0, .y = 0,}
  };
  // Specify viewport info
  VkPipelineViewportStateCreateInfo viewportInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .viewportCount = 1,
      .pViewports = &viewports,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  // Specify multisample info
  VkSampleMask sampleMask = ~0u;
  VkPipelineMultisampleStateCreateInfo multisampleInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = nullptr,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 0,
      .pSampleMask = &sampleMask,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };

  // Specify color blend state
  VkPipelineColorBlendAttachmentState attachmentStates {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
  };
  VkPipelineColorBlendStateCreateInfo colorBlendInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &attachmentStates,
      .flags = 0,
  };

  // Specify rasterizer info
  VkPipelineRasterizationStateCreateInfo rasterInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = nullptr,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1,
  };

  // Specify input assembler state
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = nullptr,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  // Specify vertex input state
  VkVertexInputBindingDescription vertex_input_bindings {
      .binding = 0,
      .stride = 5 * sizeof(float),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  VkVertexInputAttributeDescription vertex_input_attributes[2] {
      {
          .binding = 0,
          .location = 0,
          .format = VK_FORMAT_R32G32B32_SFLOAT,
          .offset = 0,
      },
      {
          .binding = 0,
          .location = 1,
          .format = VK_FORMAT_R32G32_SFLOAT,
          .offset = sizeof(float) * 3,
      }
  };
  VkPipelineVertexInputStateCreateInfo vertexInputInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &vertex_input_bindings,
      .vertexAttributeDescriptionCount = 2,
      .pVertexAttributeDescriptions = vertex_input_attributes,
  };

  // Create the pipeline cache
  VkPipelineCacheCreateInfo pipelineCacheInfo {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
      .pNext = nullptr,
      .initialDataSize = 0,
      .pInitialData = nullptr,
      .flags = 0,  // reserved, must be 0
  };

  CALL_VK(vkCreatePipelineCache(device.device_, &pipelineCacheInfo,
                                nullptr, &gfxPipeline.cache));

  // Create the pipeline
  VkGraphicsPipelineCreateInfo pipelineCreateInfo {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInputInfo,
      .pInputAssemblyState = &inputAssemblyInfo,
      .pTessellationState = nullptr,
      .pViewportState = &viewportInfo,
      .pRasterizationState = &rasterInfo,
      .pMultisampleState = &multisampleInfo,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &colorBlendInfo,
      .pDynamicState = &dynamicStateInfo,
      .layout = gfxPipeline.layout,
      .renderPass = render.renderPass_,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = 0,
  };

  VkResult pipelineResult =
      vkCreateGraphicsPipelines(device.device_, gfxPipeline.cache, 1,
          &pipelineCreateInfo, nullptr, &gfxPipeline.pipeline);

  // We don't need the shaders anymore, we can release their memory
  vkDestroyShaderModule(device.device_, vertexShader, nullptr);
  vkDestroyShaderModule(device.device_, fragmentShader, nullptr);

  return pipelineResult;
}

void DeleteGraphicsPipeline(void) {
  if (gfxPipeline.pipeline == VK_NULL_HANDLE)
     return;
  vkDestroyPipeline(device.device_, gfxPipeline.pipeline, nullptr);
  vkDestroyPipelineCache(device.device_,gfxPipeline.cache, nullptr);
  vkFreeDescriptorSets(device.device_, gfxPipeline.descPool,
       1, &gfxPipeline.descSet);
  vkDestroyDescriptorPool(device.device_, gfxPipeline.descPool, nullptr);
  vkDestroyPipelineLayout(device.device_, gfxPipeline.layout, nullptr);
}

// initialize descriptor set
VkResult  CreateDescriptorSet() {
  const VkDescriptorPoolSize type_count = {
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = TUTORIAL_TEXTURE_COUNT,
  };
  const VkDescriptorPoolCreateInfo descriptor_pool = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &type_count,
  };

  CALL_VK(vkCreateDescriptorPool(device.device_, &descriptor_pool,
              nullptr, &gfxPipeline.descPool));

  VkDescriptorSetAllocateInfo alloc_info {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = nullptr,
      .descriptorPool = gfxPipeline.descPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &gfxPipeline.dscLayout };
  CALL_VK(vkAllocateDescriptorSets(device.device_, &alloc_info,
              &gfxPipeline.descSet));

  VkDescriptorImageInfo texDsts[TUTORIAL_TEXTURE_COUNT];
  memset(texDsts, 0, sizeof(texDsts));
  for (int32_t idx = 0; idx < TUTORIAL_TEXTURE_COUNT; idx++) {
    texDsts[idx].sampler = textures[idx].sampler;
    texDsts[idx].imageView = textures[idx].view;
    texDsts[idx].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }

  VkWriteDescriptorSet writeDst {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = nullptr,
      .dstSet = gfxPipeline.descSet,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = TUTORIAL_TEXTURE_COUNT,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = texDsts,
      .pBufferInfo = nullptr,
      .pTexelBufferView = nullptr };
  vkUpdateDescriptorSets(device.device_, 1, &writeDst, 0, nullptr);
  return VK_SUCCESS;
}

uint32_t* camera_buffer;

// InitVulkan:
//   Initialize Vulkan Context when android application window is created
//   upon return, vulkan is ready to draw frames
bool InitVulkan(android_app* app) {
  androidAppCtx = app;

  if (!InitVulkan()) {
    LOGW("Vulkan is unavailable, install vulkan and re-start");
    return false;
  }

  camera_buffer = (uint32_t*)malloc(725 * 725 * sizeof(uint32_t));

  VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .apiVersion = VK_MAKE_VERSION(1, 0, 0),
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .pApplicationName = "tutorial05_triangle_window",
      .pEngineName = "tutorial",
  };

  // create a device
  CreateVulkanDevice(app->window, &appInfo);

  CreateSwapChain();

  // -----------------------------------------------------------------
  // Create render pass
  VkAttachmentDescription attachmentDescriptions {
      .format = swapchain.displayFormat_,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference colourReference = {
      .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpassDescription {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .flags = 0,
      .inputAttachmentCount = 0,
      .pInputAttachments = nullptr,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colourReference,
      .pResolveAttachments = nullptr,
      .pDepthStencilAttachment = nullptr,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = nullptr,
  };
  VkRenderPassCreateInfo renderPassCreateInfo {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = nullptr,
      .attachmentCount = 1,
      .pAttachments = &attachmentDescriptions,
      .subpassCount = 1,
      .pSubpasses = &subpassDescription,
      .dependencyCount = 0,
      .pDependencies = nullptr,
  };
  CALL_VK(vkCreateRenderPass(device.device_, &renderPassCreateInfo,
                             nullptr, &render.renderPass_));

  CreateFrameBuffers(render.renderPass_);
  CreateTexture();
  CreateBuffers();

  // Create graphics pipeline
  CreateGraphicsPipeline();

  CreateDescriptorSet();

  // -----------------------------------------------
  // Create a pool of command buffers to allocate command buffer from
  VkCommandPoolCreateInfo cmdPoolCreateInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = 0,
  };
  CALL_VK(vkCreateCommandPool(device.device_, &cmdPoolCreateInfo,
                              nullptr, &render.cmdPool_));

  // Record a command buffer that just clear the screen
  // 1 command buffer draw in 1 framebuffer
  // In our case we need 2 command as we have 2 framebuffer
  render.cmdBufferLen_ = swapchain.swapchainLength_;
  render.cmdBuffer_ = new VkCommandBuffer[swapchain.swapchainLength_];
  VkCommandBufferAllocateInfo cmdBufferCreateInfo {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = render.cmdPool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = render.cmdBufferLen_,
  };
  CALL_VK(vkAllocateCommandBuffers(device.device_,
                  &cmdBufferCreateInfo,
                  render.cmdBuffer_));

  for (int bufferIndex = 0; bufferIndex < swapchain.swapchainLength_;
       bufferIndex++) {
    // We start by creating and declare the "beginning" our command buffer
    VkCommandBufferBeginInfo cmdBufferBeginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr,
    };
    CALL_VK(vkBeginCommandBuffer(render.cmdBuffer_[bufferIndex],
                                 &cmdBufferBeginInfo));

    // Now we start a renderpass. Any draw command has to be recorded in a
    // renderpass
    VkClearValue clearVals {
        .color.float32[0] = 1.0f,
        .color.float32[1] = 0.0f,
        .color.float32[2] = 0.0f,
        .color.float32[3] = 1.0f,
    };
    VkRenderPassBeginInfo renderPassBeginInfo {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = render.renderPass_,
        .framebuffer = swapchain.framebuffers_[bufferIndex],
        .renderArea = {
             .offset = { .x = 0, .y = 0,},
             .extent = swapchain.displaySize_
        },
        .clearValueCount = 1,
        .pClearValues = &clearVals
    };
    vkCmdBeginRenderPass(render.cmdBuffer_[bufferIndex], &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);
    // Bind what is necessary to the command buffer
    vkCmdBindPipeline(render.cmdBuffer_[bufferIndex],
        VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.pipeline);
    vkCmdBindDescriptorSets(render.cmdBuffer_[bufferIndex],
        VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.layout,
        0, 1, &gfxPipeline.descSet, 0, nullptr);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(render.cmdBuffer_[bufferIndex], 0, 1,
        &buffers.vertexBuf, &offset);

    // Draw Triangle
    vkCmdDraw(render.cmdBuffer_[bufferIndex], 6, 1, 0, 0);

    vkCmdEndRenderPass(render.cmdBuffer_[bufferIndex]);
    CALL_VK(vkEndCommandBuffer(render.cmdBuffer_[bufferIndex]));
  }

  // We need to create a fence to be able, in the main loop, to wait for our
  // draw command(s) to finish before swapping the framebuffers
  VkFenceCreateInfo fenceCreateInfo {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  CALL_VK(vkCreateFence(device.device_, &fenceCreateInfo,
                        nullptr, &render.fence_));

  // We need to create a semaphore to be able to wait, in the main loop, for our
  // framebuffer to be available for us before drawing.
  VkSemaphoreCreateInfo semaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
  };
  CALL_VK(vkCreateSemaphore(device.device_, &semaphoreCreateInfo,
                            nullptr, &render.semaphore_));

  device.initialized_ = true;
  return true;
}

// InitCamera:
// Sets camera and get a working capture request
void InitCamera() {

  m_native_camera = new NativeCamera(m_selected_camera_type);

  m_native_camera->MatchCaptureSizeRequest(&m_view, 720, 480);

  ASSERT(m_view.width && m_view.height, "Could not find supportable resolution");

  m_image_reader = new ImageReader(&m_view, AIMAGE_FORMAT_YUV_420_888);
  m_image_reader->SetPresentRotation(m_native_camera->GetOrientation());

  ANativeWindow* image_reader_window = m_image_reader->GetNativeWindow();

  m_camera_ready = m_native_camera->CreateCaptureSession(image_reader_window);

 // m_image_reader->SetImageVk(&VulkanDrawFrame);

}

// IsVulkanReady():
//    native app poll to see if we are ready to draw...
bool IsVulkanReady(void) {
  return device.initialized_;
}

void DeleteSwapChain() {
  for (int i = 0; i < swapchain.swapchainLength_; i++) {
    vkDestroyFramebuffer(device.device_, swapchain.framebuffers_[i], nullptr);
    vkDestroyImageView(device.device_, swapchain.displayViews_[i], nullptr);
  }
  delete[] swapchain.framebuffers_;
  delete[] swapchain.displayViews_;

  vkDestroySwapchainKHR(device.device_, swapchain.swapchain_, nullptr);
}

void DeleteVulkan() {
  vkFreeCommandBuffers(device.device_, render.cmdPool_,
          render.cmdBufferLen_, render.cmdBuffer_);
  delete[] render.cmdBuffer_;

  vkDestroyCommandPool(device.device_, render.cmdPool_, nullptr);
  vkDestroyRenderPass(device.device_, render.renderPass_, nullptr);
  DeleteSwapChain();
  DeleteGraphicsPipeline();
  DeleteBuffers();

  vkDestroyDevice(device.device_, nullptr);
  vkDestroyInstance(device.instance_, nullptr);

  device.initialized_ = false;
}

// Draw one frame
bool VulkanDrawFrame(void) {

  if (m_image_reader->GetBufferCount() == 0) {
    return false;
  } else {
    m_image_reader->DecBufferCount();
  }

  m_image = m_image_reader->GetLatestImage();
  m_image_reader->DisplayImage(camera_buffer, m_image);

  uint32_t nextIndex;
  // Get the framebuffer index we should draw in
  CALL_VK(vkAcquireNextImageKHR(device.device_, swapchain.swapchain_,
                              UINT64_MAX, render.semaphore_,
                              VK_NULL_HANDLE, &nextIndex));
  CALL_VK(vkResetFences(device.device_, 1, &render.fence_));

  VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render.semaphore_,
        .pWaitDstStageMask = &waitStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &render.cmdBuffer_[nextIndex],
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
  };
  CALL_VK(vkQueueSubmit(device.queue_, 1, &submit_info, render.fence_));
  CALL_VK(vkWaitForFences(device.device_, 1, &render.fence_, VK_TRUE, 100000000));

  //LOGI("Drawing frames......");

  VkResult result;
  VkPresentInfoKHR presentInfo {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .swapchainCount = 1,
        .pSwapchains = &swapchain.swapchain_,
        .pImageIndices = &nextIndex,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pResults = &result,
  };
  vkQueuePresentKHR(device.queue_, &presentInfo);
  return true;
}

