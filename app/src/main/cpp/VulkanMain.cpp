#include <android/log.h>
#include <malloc.h>
#include <cassert>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb/stb_image.h>
#include "CreateShaderModule.h"
#include "VulkanMain.h"
#include "vulkan_wrapper.h"
#include "ValidationLayers.h"

// Global Variables ...
struct VulkanDeviceInfo {
    bool initialized;
    VkInstance instance;
    VkPhysicalDevice gpuDevice;
    VkPhysicalDeviceMemoryProperties gpuMemoryProperties;
    VkDevice device;
    VkSurfaceKHR surface;
    VkQueue queue;
};
VulkanDeviceInfo device;

struct VulkanSwapchainInfo {
    VkSwapchainKHR swapchain;
    uint32_t swapchainLength;
    VkExtent2D displaySize;
    VkFormat displayFormat;
    VkFramebuffer* framebuffers;  // array of frame buffers and views
    VkImageView* imageViews;
};
VulkanSwapchainInfo swapchain;

typedef struct texture_object {
    VkSampler sampler;
    VkImage image;
    VkImageLayout imageLayout;
    VkDeviceMemory memory;
    VkImageView view;
    int32_t texWidth, texHeight;
} texture_object;

#define VARTIP_TEXTURE_COUNT 1
static const VkFormat kTextureFormat = VK_FORMAT_R8G8B8A8_UNORM;
struct texture_object textures[VARTIP_TEXTURE_COUNT];

struct VulkanBufferInfo {
    VkBuffer vertexBuffer;
};
VulkanBufferInfo buffers;

struct VulkanGfxPipelineInfo {
    VkDescriptorSetLayout descriptorLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;
    VkPipelineLayout layout;
    VkPipelineCache cache;
    VkPipeline pipeline;
};
VulkanGfxPipelineInfo gfxPipeline;

struct VulkanRenderInfo {
    VkRenderPass renderPass;
    VkCommandPool cmdPool;
    VkCommandBuffer* cmdBuffer;
    uint32_t cmdBufferLength;
    VkSemaphore semaphore;
    VkFence fence;
};
VulkanRenderInfo render;

void* mappedData;

// Camera variables
NativeCamera* m_nativeCamera;
// Image Reader
ImageFormat m_view{0, 0, 0};
ImageReader* m_imageReader;
AImage* m_image;
volatile bool m_cameraReady;
VkDebugReportCallbackEXT debugCallbackHandle;

// Android Native App pointer...
android_app* androidAppCtx = nullptr;


// Create vulkan device
void CreateVulkanDevice(ANativeWindow* platformWindow, VkApplicationInfo* appInfo) {
    std::vector<const char*> instanceExtensions;
    std::vector<const char*> instanceLayers;
    std::vector<const char*> deviceExtensions;

    instanceExtensions.push_back("VK_KHR_surface");
    instanceExtensions.push_back("VK_KHR_android_surface");
    deviceExtensions.push_back("VK_KHR_swapchain");

#if (VARTIP_VALIDATION_LAYERS)
    instanceExtensions.push_back("VK_EXT_debug_report");
    // VK_GOOGLE_THREADING_LAYER better to be the very first one
    // VK_LAYER_GOOGLE_unique_objects need to be after VK_LAYER_LUNARG_core_validation
    instanceLayers.push_back("VK_LAYER_GOOGLE_threading");
    instanceLayers.push_back("VK_LAYER_LUNARG_core_validation");
    instanceLayers.push_back("VK_LAYER_GOOGLE_unique_objects");
    instanceLayers.push_back("VK_LAYER_LUNARG_object_tracker");
    instanceLayers.push_back("VK_LAYER_LUNARG_parameter_validation");
#endif

    // Create the Vulkan instance
    VkInstanceCreateInfo instanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .pApplicationInfo = appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data(),
        .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
        .ppEnabledLayerNames = instanceLayers.data(),
    };
    CALL_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &device.instance));

#if (VARTIP_VALIDATION_LAYERS)
    CreateDebugReportExt(device.instance, &debugCallbackHandle);
#endif

    VkAndroidSurfaceCreateInfoKHR createInfo{.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                                             .pNext = nullptr,
                                             .flags = 0,
                                             .window = platformWindow};

    CALL_VK(vkCreateAndroidSurfaceKHR(device.instance, &createInfo, nullptr, &device.surface));

    // Find one GPU to use:
    // On Android, every GPU device is equal -- supporting
    // graphics/compute/present for this sample, we use the very first GPU device
    // found on the system
    uint32_t gpuCount = 0;
    CALL_VK(vkEnumeratePhysicalDevices(device.instance, &gpuCount, nullptr));
    VkPhysicalDevice tmpGpus[gpuCount];
    CALL_VK(vkEnumeratePhysicalDevices(device.instance, &gpuCount, tmpGpus));
    device.gpuDevice = tmpGpus[0];  // Pick up the first GPU Device

    vkGetPhysicalDeviceMemoryProperties(device.gpuDevice, &device.gpuMemoryProperties);

    // Create a logical device (vulkan device)
    float priorities[] = {
        1.0f,
    };
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCount = 1,
        .queueFamilyIndex = 0,
        .pQueuePriorities = priorities,
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = nullptr,
    };

    CALL_VK(vkCreateDevice(device.gpuDevice, &deviceCreateInfo, nullptr, &device.device));
    vkGetDeviceQueue(device.device, 0, 0, &device.queue);
}

void CreateSwapChain() {
    memset(&swapchain, 0, sizeof(swapchain));

    // Get the surface capabilities because:
    //   - It contains the minimal and max length of the chain, we will need it
    //   - It's necessary to query the supported surface format (R8G8B8A8 for instance ...)
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.gpuDevice, device.surface, &surfaceCapabilities);
    // Query the list of supported surface format and choose one we like
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice, device.surface, &formatCount, nullptr);
    VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[formatCount];
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice, device.surface, &formatCount, formats);
    LOGI("Got %d formats", formatCount);

    uint32_t chosenFormat;
    for (chosenFormat = 0; chosenFormat < formatCount; chosenFormat++) {
        if (formats[chosenFormat].format == VK_FORMAT_R8G8B8A8_UNORM) break;
    }
    assert(chosenFormat < formatCount);

    swapchain.displaySize = surfaceCapabilities.currentExtent;
    swapchain.displayFormat = formats[chosenFormat].format;
    // Create a swap chain (here we choose the minimum available number of surface
    // in the chain)
    uint32_t queueFamily = 0;
    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .surface = device.surface,
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
    CALL_VK(vkCreateSwapchainKHR(device.device, &swapchainCreateInfo, nullptr, &swapchain.swapchain));

    // Get the length of the created swap chain
    CALL_VK(vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &swapchain.swapchainLength, nullptr));
    delete[] formats;
}

void CreateFrameBuffers(VkRenderPass& renderPass, VkImageView depthView = VK_NULL_HANDLE) {
    // query display attachment to swapchain
    uint32_t SwapchainImagesCount = 0;
    CALL_VK(vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &SwapchainImagesCount, nullptr));
    VkImage* displayImages = new VkImage[SwapchainImagesCount];
    CALL_VK(vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &SwapchainImagesCount, displayImages));

    // create image view for each swapchain image
    swapchain.imageViews = new VkImageView[SwapchainImagesCount];
    for (uint32_t i = 0; i < SwapchainImagesCount; i++) {
        VkImageViewCreateInfo viewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .image = displayImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain.displayFormat,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_R,
                    .g = VK_COMPONENT_SWIZZLE_G,
                    .b = VK_COMPONENT_SWIZZLE_B,
                    .a = VK_COMPONENT_SWIZZLE_A,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .flags = 0,
        };
        CALL_VK(vkCreateImageView(device.device, &viewCreateInfo, nullptr, &swapchain.imageViews[i]));
    }
    delete[] displayImages;

    // create a framebuffer from each swapchain image
    swapchain.framebuffers = new VkFramebuffer[swapchain.swapchainLength];
    for (uint32_t i = 0; i < swapchain.swapchainLength; i++) {
        VkImageView attachments[2] = {
            swapchain.imageViews[i],
            depthView,
        };
        VkFramebufferCreateInfo fbCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .renderPass = renderPass,
            .layers = 1,
            .attachmentCount = 1,  // 2 if using depth
            .pAttachments = attachments,
            .width = static_cast<uint32_t>(swapchain.displaySize.width),
            .height = static_cast<uint32_t>(swapchain.displaySize.height),
        };
        fbCreateInfo.attachmentCount = (depthView == VK_NULL_HANDLE ? 1 : 2);

        CALL_VK(vkCreateFramebuffer(device.device, &fbCreateInfo, nullptr, &swapchain.framebuffers[i]));
    }
}

// A help function to map required memory property into a VK memory type
// memory type is an index into the array of 32 entries; or the bit index
// for the memory type ( each BIT of an 32 bit integer is a type ).
VkResult AllocateMemoryTypeFromProperties(uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex) {
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((device.gpuMemoryProperties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return VK_SUCCESS;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return VK_ERROR_MEMORY_MAP_FAILED;
}

uint32_t imgWidth = 480;
uint32_t imgHeight = 720;
uint32_t n = 4;
uint32_t rowPitch = 1920;

VkResult LoadTextureFromCamera(struct texture_object* textureObj, VkImageUsageFlags usage, VkFlags requiredProps) {
    if (!(usage | requiredProps)) {
        __android_log_print(ANDROID_LOG_ERROR, "texture", "No usage and required_pros");
        return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }

    // Check for linear supportability
    VkFormatProperties props;
    bool needBlit = true;
    vkGetPhysicalDeviceFormatProperties(device.gpuDevice, kTextureFormat, &props);
    assert((props.linearTilingFeatures | props.optimalTilingFeatures) & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

    if (props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
        // linear format supporting the required texture
        needBlit = false;
    }

    unsigned char* imageData = (unsigned char*)malloc(n * imgHeight * imgHeight);

    textureObj->texWidth = imgWidth;
    textureObj->texHeight = imgHeight;

    // Allocate the linear texture so texture could be copied over
    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = kTextureFormat,
        .extent = {static_cast<uint32_t>(imgWidth), static_cast<uint32_t>(imgHeight), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = (needBlit ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : VK_IMAGE_USAGE_SAMPLED_BIT),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .flags = 0,
    };
    VkMemoryAllocateInfo memAlloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = 0,
        .memoryTypeIndex = 0,
    };

    VkMemoryRequirements mem_reqs;
    CALL_VK(vkCreateImage(device.device, &image_create_info, nullptr, &textureObj->image));
    vkGetImageMemoryRequirements(device.device, textureObj->image, &mem_reqs);
    memAlloc.allocationSize = mem_reqs.size;
    VK_CHECK(AllocateMemoryTypeFromProperties(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                              &memAlloc.memoryTypeIndex));
    CALL_VK(vkAllocateMemory(device.device, &memAlloc, nullptr, &textureObj->memory));
    CALL_VK(vkBindImageMemory(device.device, textureObj->image, textureObj->memory, 0));

    if (requiredProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        const VkImageSubresource subres = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .arrayLayer = 0,
        };
        VkSubresourceLayout layout;
        // void* data;

        vkGetImageSubresourceLayout(device.device, textureObj->image, &subres, &layout);
        CALL_VK(vkMapMemory(device.device, textureObj->memory, 0, memAlloc.allocationSize, 0, &mappedData));

        LOGI("RowPitch = %d", (int)layout.rowPitch);

        rowPitch = layout.rowPitch;

        for (int32_t y = 0; y < imgHeight; y++) {
            unsigned char* row = (unsigned char*)((char*)mappedData + layout.rowPitch * y);
            for (int32_t x = 0; x < imgWidth; x++) {
                row[x * 4] = imageData[(x + y * imgWidth) * 4];
                row[x * 4 + 1] = imageData[(x + y * imgWidth) * 4 + 1];
                row[x * 4 + 2] = imageData[(x + y * imgWidth) * 4 + 2];
                row[x * 4 + 3] = imageData[(x + y * imgWidth) * 4 + 3];
            }
        }

        // vkUnmapMemory(device.device, textureObj->memory);
        free(imageData);
        // stbi_image_free(imageData);
    }
    // delete [] fileContent;

    textureObj->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // If linear is supported, we are done
    if (needBlit == false) {
        return VK_SUCCESS;
    }

    // save current image and mem as staging image and memory
    VkImage stageImage = textureObj->image;
    VkDeviceMemory stageMem = textureObj->memory;
    textureObj->image = VK_NULL_HANDLE;
    textureObj->memory = VK_NULL_HANDLE;

    // Create a tile texture to blit into
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    CALL_VK(vkCreateImage(device.device, &image_create_info, nullptr, &textureObj->image));
    vkGetImageMemoryRequirements(device.device, textureObj->image, &mem_reqs);

    memAlloc.allocationSize = mem_reqs.size;
    VK_CHECK(AllocateMemoryTypeFromProperties(mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                              &memAlloc.memoryTypeIndex));
    CALL_VK(vkAllocateMemory(device.device, &memAlloc, nullptr, &textureObj->memory));
    CALL_VK(vkBindImageMemory(device.device, textureObj->image, textureObj->memory, 0));

    VkCommandPoolCreateInfo cmdPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = 0,
    };

    VkCommandPool cmdPool;
    CALL_VK(vkCreateCommandPool(device.device, &cmdPoolCreateInfo, nullptr, &cmdPool));

    VkCommandBuffer gfxCmd;
    const VkCommandBufferAllocateInfo cmd = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = cmdPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    CALL_VK(vkAllocateCommandBuffers(device.device, &cmd, &gfxCmd));
    VkCommandBufferBeginInfo cmd_buf_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
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
    vkCmdCopyImage(gfxCmd, stageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, textureObj->image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bltInfo);

    CALL_VK(vkEndCommandBuffer(gfxCmd));
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VkFence fence;
    CALL_VK(vkCreateFence(device.device, &fenceInfo, nullptr, &fence));

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
    CALL_VK(vkQueueSubmit(device.queue, 1, &submitInfo, fence) != VK_SUCCESS);
    CALL_VK(vkWaitForFences(device.device, 1, &fence, VK_TRUE, 100000000) != VK_SUCCESS);
    vkDestroyFence(device.device, fence, nullptr);

    vkFreeCommandBuffers(device.device, cmdPool, 1, &gfxCmd);
    vkDestroyCommandPool(device.device, cmdPool, nullptr);
    vkDestroyImage(device.device, stageImage, nullptr);
    vkFreeMemory(device.device, stageMem, nullptr);
    return VK_SUCCESS;
}

void CreateTexture() {
    for (uint32_t i = 0; i < VARTIP_TEXTURE_COUNT; i++) {
        LoadTextureFromCamera(&textures[i], VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

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
            .format = kTextureFormat,
            .components =
                {
                    VK_COMPONENT_SWIZZLE_R,
                    VK_COMPONENT_SWIZZLE_G,
                    VK_COMPONENT_SWIZZLE_B,
                    VK_COMPONENT_SWIZZLE_A,
                },
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
            .flags = 0,
        };

        CALL_VK(vkCreateSampler(device.device, &sampler, nullptr, &textures[i].sampler));
        view.image = textures[i].image;
        CALL_VK(vkCreateImageView(device.device, &view, nullptr, &textures[i].view));
    }
}

// A helper function
bool MapMemoryTypeToIndex(uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(device.gpuDevice, &memoryProperties);
    // Search memtypes to find first index with those properties
    for (uint32_t i = 0; i < 32; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((memoryProperties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
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
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
    };

    // Create a vertex buffer
    uint32_t queueIdx = 0;
    VkBufferCreateInfo createBufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = sizeof(vertexData),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .flags = 0,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .pQueueFamilyIndices = &queueIdx,
        .queueFamilyIndexCount = 1,
    };

    CALL_VK(vkCreateBuffer(device.device, &createBufferInfo, nullptr, &buffers.vertexBuffer));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device.device, buffers.vertexBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = sizeof(vertexData),
        .memoryTypeIndex = 0,  // Memory type assigned in the next step
    };

    // Assign the proper memory type for that buffer
    allocInfo.allocationSize = memReq.size;
    MapMemoryTypeToIndex(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &allocInfo.memoryTypeIndex);

    // Allocate memory for the buffer
    VkDeviceMemory deviceMemory;
    CALL_VK(vkAllocateMemory(device.device, &allocInfo, nullptr, &deviceMemory));

    void* data;
    CALL_VK(vkMapMemory(device.device, deviceMemory, 0, sizeof(vertexData), 0, &data));
    memcpy(data, vertexData, sizeof(vertexData));
    vkUnmapMemory(device.device, deviceMemory);

    CALL_VK(vkBindBufferMemory(device.device, buffers.vertexBuffer, deviceMemory, 0));
    return true;
}

void DeleteBuffers(void) { vkDestroyBuffer(device.device, buffers.vertexBuffer, nullptr); }

// Create Graphics Pipeline
VkResult CreateGraphicsPipeline() {
    memset(&gfxPipeline, 0, sizeof(gfxPipeline));

    const VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = VARTIP_TEXTURE_COUNT,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr,
    };
    const VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = 1,
        .pBindings = &descriptorSetLayoutBinding,
    };
    CALL_VK(vkCreateDescriptorSetLayout(device.device, &descriptorSetLayoutCreateInfo, nullptr,
                                        &gfxPipeline.descriptorLayout));
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = 1,
        .pSetLayouts = &gfxPipeline.descriptorLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    CALL_VK(vkCreatePipelineLayout(device.device, &pipelineLayoutCreateInfo, nullptr, &gfxPipeline.layout));

    // No dynamic state in that tutorial
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                      .pNext = nullptr,
                                                      .dynamicStateCount = 0,
                                                      .pDynamicStates = nullptr};

    // Specify vertex and fragment shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2]{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = LoadSPIRVShader(androidAppCtx, "shaders/tri.vert.spv", device.device),
            .pSpecializationInfo = nullptr,
            .flags = 0,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = LoadSPIRVShader(androidAppCtx, "shaders/tri.frag.spv", device.device),
            .pSpecializationInfo = nullptr,
            .flags = 0,
            .pName = "main",
        }};

    VkViewport viewports{
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
        .x = 0,
        .y = 0,
        .width = (float)swapchain.displaySize.width,
        .height = (float)swapchain.displaySize.height,
    };

    VkRect2D scissor = {.extent = swapchain.displaySize,
                        .offset = {
                            .x = 0,
                            .y = 0,
                        }};
    // Specify viewport info
    VkPipelineViewportStateCreateInfo viewportInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .viewportCount = 1,
        .pViewports = &viewports,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    // Specify multisample info
    VkSampleMask sampleMask = ~0u;
    VkPipelineMultisampleStateCreateInfo multisampleInfo{
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
    VkPipelineColorBlendAttachmentState attachmentStates{
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
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
    VkPipelineRasterizationStateCreateInfo rasterInfo{
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
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Specify vertex input state
    VkVertexInputBindingDescription vertex_input_bindings{
        .binding = 0,
        .stride = 5 * sizeof(float),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription vertex_input_attributes[2]{{
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
                                                                 }};
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_input_bindings,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = vertex_input_attributes,
    };

    // Create the pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
        .pNext = nullptr,
        .initialDataSize = 0,
        .pInitialData = nullptr,
        .flags = 0,  // reserved, must be 0
    };

    CALL_VK(vkCreatePipelineCache(device.device, &pipelineCacheInfo, nullptr, &gfxPipeline.cache));

    // Create the pipeline
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{
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
        .renderPass = render.renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VkResult pipelineResult = vkCreateGraphicsPipelines(device.device, gfxPipeline.cache, 1, &pipelineCreateInfo,
                                                        nullptr, &gfxPipeline.pipeline);

    // We don't need the shaders anymore, we can release their memory
    vkDestroyShaderModule(device.device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device.device, shaderStages[1].module, nullptr);

    return pipelineResult;
}

void DeleteGraphicsPipeline(void) {
    if (gfxPipeline.pipeline == VK_NULL_HANDLE) return;
    vkDestroyPipeline(device.device, gfxPipeline.pipeline, nullptr);
    vkDestroyPipelineCache(device.device, gfxPipeline.cache, nullptr);
    vkFreeDescriptorSets(device.device, gfxPipeline.descriptorPool, 1, &gfxPipeline.descriptorSet);
    vkDestroyDescriptorPool(device.device, gfxPipeline.descriptorPool, nullptr);
    vkDestroyPipelineLayout(device.device, gfxPipeline.layout, nullptr);
}

// initialize descriptor set
VkResult CreateDescriptorSet() {
    const VkDescriptorPoolSize type_count = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = VARTIP_TEXTURE_COUNT,
    };
    const VkDescriptorPoolCreateInfo descriptor_pool = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &type_count,
    };

    CALL_VK(vkCreateDescriptorPool(device.device, &descriptor_pool, nullptr, &gfxPipeline.descriptorPool));

    VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .pNext = nullptr,
                                           .descriptorPool = gfxPipeline.descriptorPool,
                                           .descriptorSetCount = 1,
                                           .pSetLayouts = &gfxPipeline.descriptorLayout};
    CALL_VK(vkAllocateDescriptorSets(device.device, &alloc_info, &gfxPipeline.descriptorSet));

    VkDescriptorImageInfo texDsts[VARTIP_TEXTURE_COUNT];
    memset(texDsts, 0, sizeof(texDsts));
    for (int32_t idx = 0; idx < VARTIP_TEXTURE_COUNT; idx++) {
        texDsts[idx].sampler = textures[idx].sampler;
        texDsts[idx].imageView = textures[idx].view;
        texDsts[idx].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }

    VkWriteDescriptorSet writeDst{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                  .pNext = nullptr,
                                  .dstSet = gfxPipeline.descriptorSet,
                                  .dstBinding = 0,
                                  .dstArrayElement = 0,
                                  .descriptorCount = VARTIP_TEXTURE_COUNT,
                                  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .pImageInfo = texDsts,
                                  .pBufferInfo = nullptr,
                                  .pTexelBufferView = nullptr};
    vkUpdateDescriptorSets(device.device, 1, &writeDst, 0, nullptr);
    return VK_SUCCESS;
}

uint32_t* cameraBuffer;

// Initialize Vulkan Context when android application window is created upon return, vulkan is ready to draw frames
bool InitVulkan(android_app* app) {
    androidAppCtx = app;

    if (InitVulkan() == false) {
        LOGW("Vulkan is unavailable, install vulkan and re-start");
        return false;
    }

    cameraBuffer = (uint32_t*)malloc(725 * 725 * sizeof(uint32_t));

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .apiVersion = VK_MAKE_VERSION(1, 0, 0),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .pApplicationName = "VARTIP",
        .pEngineName = "null",
    };

    // create a device
    CreateVulkanDevice(app->window, &appInfo);

    CreateSwapChain();

    // Create render pass
    VkAttachmentDescription attachmentDescriptions{
        .format = swapchain.displayFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colourReference = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription{
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
    VkRenderPassCreateInfo renderPassCreateInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .attachmentCount = 1,
        .pAttachments = &attachmentDescriptions,
        .subpassCount = 1,
        .pSubpasses = &subpassDescription,
        .dependencyCount = 0,
        .pDependencies = nullptr,
    };
    CALL_VK(vkCreateRenderPass(device.device, &renderPassCreateInfo, nullptr, &render.renderPass));

    CreateFrameBuffers(render.renderPass);
    CreateTexture();
    CreateBuffers();

    // Create graphics pipeline
    CreateGraphicsPipeline();

    CreateDescriptorSet();

    // -----------------------------------------------
    // Create a pool of command buffers to allocate command buffer from
    VkCommandPoolCreateInfo cmdPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = 0,
    };
    CALL_VK(vkCreateCommandPool(device.device, &cmdPoolCreateInfo, nullptr, &render.cmdPool));

    // Record a command buffer that just clear the screen
    // 1 command buffer draw in 1 framebuffer
    // In our case we need 2 command as we have 2 framebuffer
    render.cmdBufferLength = swapchain.swapchainLength;
    render.cmdBuffer = new VkCommandBuffer[swapchain.swapchainLength];
    VkCommandBufferAllocateInfo cmdBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = render.cmdPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = render.cmdBufferLength,
    };
    CALL_VK(vkAllocateCommandBuffers(device.device, &cmdBufferCreateInfo, render.cmdBuffer));

    for (int bufferIndex = 0; bufferIndex < swapchain.swapchainLength; bufferIndex++) {
        // We start by creating and declare the "beginning" our command buffer
        VkCommandBufferBeginInfo cmdBufferBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pInheritanceInfo = nullptr,
        };
        CALL_VK(vkBeginCommandBuffer(render.cmdBuffer[bufferIndex], &cmdBufferBeginInfo));

        // Now we start a renderpass. Any draw command has to be recorded in a
        // renderpass
        VkClearValue clearValues{
            .color.float32[0] = 1.0f,
            .color.float32[1] = 0.0f,
            .color.float32[2] = 0.0f,
            .color.float32[3] = 1.0f,
        };
        VkRenderPassBeginInfo renderPassBeginInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                  .pNext = nullptr,
                                                  .renderPass = render.renderPass,
                                                  .framebuffer = swapchain.framebuffers[bufferIndex],
                                                  .renderArea = {.offset =
                                                                     {
                                                                         .x = 0,
                                                                         .y = 0,
                                                                     },
                                                                 .extent = swapchain.displaySize},
                                                  .clearValueCount = 1,
                                                  .pClearValues = &clearValues};
        vkCmdBeginRenderPass(render.cmdBuffer[bufferIndex], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        // Bind what is necessary to the command buffer
        vkCmdBindPipeline(render.cmdBuffer[bufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.pipeline);
        vkCmdBindDescriptorSets(render.cmdBuffer[bufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline.layout, 0,
                                1, &gfxPipeline.descriptorSet, 0, nullptr);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(render.cmdBuffer[bufferIndex], 0, 1, &buffers.vertexBuffer, &offset);

        // Draw quad
        vkCmdDraw(render.cmdBuffer[bufferIndex], 6, 1, 0, 0);

        vkCmdEndRenderPass(render.cmdBuffer[bufferIndex]);
        CALL_VK(vkEndCommandBuffer(render.cmdBuffer[bufferIndex]));
    }

    // We need to create a fence to be able, in the main loop, to wait for our
    // draw command(s) to finish before swapping the framebuffers
    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    CALL_VK(vkCreateFence(device.device, &fenceCreateInfo, nullptr, &render.fence));

    // We need to create a semaphore to be able to wait, in the main loop, for our
    // framebuffer to be available for us before drawing.
    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    CALL_VK(vkCreateSemaphore(device.device, &semaphoreCreateInfo, nullptr, &render.semaphore));

    device.initialized = true;
    return true;
}

// InitCamera:
// Sets camera and get a working capture request
void InitCamera() {
    m_nativeCamera = new NativeCamera();

    m_nativeCamera->MatchCaptureSizeRequest(&m_view, 720, 480);

    ASSERT(m_view.width && m_view.height, "Could not find supportable resolution");

    m_imageReader = new ImageReader(&m_view, AIMAGE_FORMAT_YUV_420_888);
    m_imageReader->SetPresentRotation(m_nativeCamera->GetOrientation());

    ANativeWindow* imageReaderWindow = m_imageReader->GetNativeWindow();

    m_cameraReady = m_nativeCamera->CreateCaptureSession(imageReaderWindow);

    // m_imageReader->SetImageVk(&VulkanDrawFrame);
}

// IsVulkanReady():
//    native app poll to see if we are ready to draw...
bool IsVulkanReady(void) { return device.initialized; }

void DeleteSwapChain() {
    for (int i = 0; i < swapchain.swapchainLength; i++) {
        vkDestroyFramebuffer(device.device, swapchain.framebuffers[i], nullptr);
        vkDestroyImageView(device.device, swapchain.imageViews[i], nullptr);
    }
    delete[] swapchain.framebuffers;
    delete[] swapchain.imageViews;

    vkDestroySwapchainKHR(device.device, swapchain.swapchain, nullptr);
}

void DeleteVulkan() {
    vkFreeCommandBuffers(device.device, render.cmdPool, render.cmdBufferLength, render.cmdBuffer);
    delete[] render.cmdBuffer;

    vkDestroyCommandPool(device.device, render.cmdPool, nullptr);
    vkDestroyRenderPass(device.device, render.renderPass, nullptr);
    DeleteSwapChain();
    DeleteGraphicsPipeline();
    DeleteBuffers();

#if (VARTIP_VALIDATION_LAYERS)
    DestroyDebugReportExt(device.instance, debugCallbackHandle);
#endif

    vkDestroyDevice(device.device, nullptr);
    vkDestroyInstance(device.instance, nullptr);

    device.initialized = false;
}
int test = 0;
// Draw one frame
#include <unistd.h>
bool VulkanDrawFrame(android_app* app) {
    if (m_imageReader->GetBufferCount() == 0) {
        return false;
    } else {
        m_imageReader->DecBufferCount();
    }

    m_image = m_imageReader->GetLatestImage();
    m_imageReader->DisplayImage(cameraBuffer, m_image);

    // Read the file:
//          AAsset* file = AAssetManager_open(androidAppCtx->activity->assetManager, "sample_tex.png", AASSET_MODE_BUFFER);
//          size_t fileLength = AAsset_getLength(file);
//          stbi_uc* fileContent = new unsigned char[fileLength];
//          AAsset_read(file, fileContent, fileLength);
//
//          unsigned char* imageData = stbi_load_from_memory(
//              fileContent, fileLength, reinterpret_cast<int*>(&imgWidth),
//              reinterpret_cast<int*>(&imgHeight), reinterpret_cast<int*>(&n), 4);
//          assert(n == 4);

    unsigned char* imageData = reinterpret_cast<unsigned char*>(cameraBuffer);

    for (int32_t y = 0; y < imgHeight; y++) {
        unsigned char* row = (unsigned char*)((char*)mappedData + rowPitch * y);
        for (int32_t x = 0; x < imgWidth; x++) {
            // When reading from File its imgWidth
            // When reading from camera it seems to be a 720x720 buffer
            // ... TODO
            row[x * 4] = imageData[(x + (y * imgHeight)) * 4];
            row[x * 4 + 1] = imageData[((x + (y * imgHeight)) * 4) + 1];
            row[x * 4 + 2] = imageData[((x + (y * imgHeight)) * 4) + 2];
            row[x * 4 + 3] = imageData[((x + (y * imgHeight)) * 4) + 3];
        }
    }

    // uint32_t* row = static_cast<uint32_t *>(mappedData);

    // memcpy(mappedData, (void*)cameraBuffer, 720 * 480 * 4);

    uint32_t nextIndex;
    // Get the framebuffer index we should draw in
    CALL_VK(vkAcquireNextImageKHR(device.device, swapchain.swapchain, UINT64_MAX, render.semaphore, VK_NULL_HANDLE,
                                  &nextIndex));
    CALL_VK(vkResetFences(device.device, 1, &render.fence));

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                .pNext = nullptr,
                                .waitSemaphoreCount = 1,
                                .pWaitSemaphores = &render.semaphore,
                                .pWaitDstStageMask = &waitStageMask,
                                .commandBufferCount = 1,
                                .pCommandBuffers = &render.cmdBuffer[nextIndex],
                                .signalSemaphoreCount = 0,
                                .pSignalSemaphores = nullptr};
    CALL_VK(vkQueueSubmit(device.queue, 1, &submit_info, render.fence));
    CALL_VK(vkWaitForFences(device.device, 1, &render.fence, VK_TRUE, 100000000));

    // LOGI("Drawing frames......");

    VkResult result;
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .swapchainCount = 1,
        .pSwapchains = &swapchain.swapchain,
        .pImageIndices = &nextIndex,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pResults = &result,
    };
    vkQueuePresentKHR(device.queue, &presentInfo);
    return true;
}
