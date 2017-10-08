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

add_library(vktuts SHARED
   ${SRC_DIR}/VulkanMain.cpp
   ${SRC_DIR}/AndroidMain.cpp
   ${SRC_DIR}/CreateShaderModule.cpp
   ${SRC_DIR}/ImageReader.cpp
   ${SRC_DIR}/NativeCamera.cpp
   ${COMMON_DIR}/vulkan_wrapper/vulkan_wrapper.cpp)

target_include_directories(vktuts PRIVATE
   ${COMMON_DIR}/vulkan_wrapper
   ${COMMON_DIR}/src
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
target_link_libraries(vktuts native_glue log android camera2ndk mediandk)

########     CDep package for shaderc    ######################
# pre-requirement:
#    in terminal, cd {project_dir}; ./cdep
# that will generate .cdep/modules/cdep-dependencies-config.cmake
# the next 2 lines will be able to pull it into the project
# shaderc binaries are saved on github:
#      https://github.com/ggfan/shaderc/releases
# the glue to reach it for CDep is in ${project_dir}/cdep.yml
# ******* Most Importantly, CDep full documentation is @:
#  https://github.com/cdep

get_filename_component(cdep-dependencies_DIR
   ${CMAKE_SOURCE_DIR}/../../../../.cdep/modules  ABSOLUTE)
find_package(cdep-dependencies REQUIRED)
add_all_cdep_dependencies(vktuts)
