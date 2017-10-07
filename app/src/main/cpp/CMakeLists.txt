cmake_minimum_required(VERSION 3.4.3)

project(VARTIP)

# build native_app_glue as a static lib
add_library(native_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

# build Vulkan app
set(SRC_DIR ${CMAKE_SOURCE_DIR})
get_filename_component(REPO_ROOT_DIR
   ${CMAKE_SOURCE_DIR}/../../../..  ABSOLUTE)
set(COMMON_DIR ${REPO_ROOT_DIR}/app/common)
set(LIBS_DIR ${REPO_ROOT_DIR}/app/libs)

add_library(vktuts SHARED
   ${SRC_DIR}/VulkanMain.cpp
   ${SRC_DIR}/AndroidMain.cpp
   ${COMMON_DIR}/vulkan_wrapper/vulkan_wrapper.cpp)

target_include_directories(vktuts PRIVATE
   ${COMMON_DIR}/vulkan_wrapper
   ${COMMON_DIR}/src
   ${LIBS_DIR}
   ${ANDROID_NDK}/sources/android/native_app_glue)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
   -std=c++11 -Werror -Wno-unused-variable -DVK_USE_PLATFORM_ANDROID_KHR")

if (${ANDROID_ABI} STREQUAL "armeabi-v7a")
   set(CMAKE_CXX_FLAGS} "${CMAKE_CXX_FLAGS} \
       -mhard-float -D_NDK_MATH_NO_SOFTFP=1 -mfloat-abi=hard")
   set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} \
       -Wl,--no-warn-mismatch")
endif()

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
target_link_libraries(vktuts native_glue log android)
