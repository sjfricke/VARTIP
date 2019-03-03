cmake_minimum_required(VERSION 3.4.3)

project(VARTIP)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library(native_glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

# Validation layers and Vulkan headers
set(VK_VAL_LAYER_SRC_DIR ${ANDROID_NDK}/sources/third_party/vulkan/src)
include_directories(${VK_VAL_LAYER_SRC_DIR}/include)

# build Vulkan app
set(SRC_DIR ${CMAKE_SOURCE_DIR})
get_filename_component(REPO_ROOT_DIR  ${CMAKE_SOURCE_DIR}/../../../.. ABSOLUTE)
set(COMMON_DIR ${REPO_ROOT_DIR}/app/common)
set(THIRD_PARTY_DIR ${REPO_ROOT_DIR}/app/third_party)

add_library(vartip SHARED
   ${SRC_DIR}/VulkanMain.cpp
   ${SRC_DIR}/AndroidMain.cpp
   ${SRC_DIR}/CreateShaderModule.cpp
   ${SRC_DIR}/ImageReader.cpp
   ${SRC_DIR}/NativeCamera.cpp
   ${SRC_DIR}/ValidationLayers.cpp
   ${COMMON_DIR}/vulkan_wrapper/vulkan_wrapper.cpp)

target_include_directories(vartip PRIVATE
   ${COMMON_DIR}/vulkan_wrapper
   ${COMMON_DIR}/src
   ${THIRD_PARTY_DIR}
   ${ANDROID_NDK}/sources/android/native_app_glue)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
   -std=c++11 -Werror -Wno-unused-variable -DVK_USE_PLATFORM_ANDROID_KHR -DUSE_DEBUG_EXTENTIONS")

if (${ANDROID_ABI} STREQUAL "armeabi-v7a")
   set(CMAKE_CXX_FLAGS} "${CMAKE_CXX_FLAGS} \
       -mhard-float -D_NDK_MATH_NO_SOFTFP=1 -mfloat-abi=hard")
   set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} \
       -Wl,--no-warn-mismatch")
endif()

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
target_link_libraries(vartip
                      native_glue
                      log
                      android
                      camera2ndk
                      mediandk)
