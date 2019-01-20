#ifndef VARTIP_UTIL_H_
#define VARTIP_UTIL_H_

#include <android/log.h>
#include <stdlib.h>
#include <unistd.h>

// used to get logcat outputs which can be regex filtered by the LOG_TAG we give
// So in Logcat you can filter this example by putting "VARTIP"
#define LOG_TAG "VARTIP"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ASSERT(cond, fmt, ...)                                    \
    if (!(cond)) {                                                \
        __android_log_assert(#cond, LOG_TAG, fmt, ##__VA_ARGS__); \
    }

// Vulkan call wrapper
#define CALL_VK(func)                                                                                                \
    if (VK_SUCCESS != (func)) {                                                                                      \
        __android_log_print(ANDROID_LOG_ERROR, "Tutorial ", "Vulkan error. File[%s], line[%d]", __FILE__, __LINE__); \
        assert(false);                                                                                               \
    }

// A macro to check value is VK_SUCCESS
// Used also for non-vulkan functions but return VK_SUCCESS
#define VK_CHECK(x) CALL_VK(x)

// A Data Structure to communicate resolution between camera and ImageReader
struct ImageFormat {
    int32_t width;
    int32_t height;
    int32_t format;  // ex) YUV_420
};

// Helps assist image size comparison, by comparing the absolute size regardless of the portrait or landscape mode
class DisplayDimension {
   public:
    DisplayDimension(int32_t width, int32_t height) : m_width(width), m_height(height), m_portrait(false) {
        if (height > width) {
            // make it landscape
            m_width = height;
            m_height = m_width;
            m_portrait = true;
        }
    }

    DisplayDimension(const DisplayDimension& other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_portrait = other.m_portrait;
    }

    DisplayDimension(void) {
        m_width = 0;
        m_height = 0;
        m_portrait = false;
    }

    DisplayDimension& operator=(const DisplayDimension& other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_portrait = other.m_portrait;
        return (*this);
    }

    bool IsSameRatio(DisplayDimension& other) { return (m_width * other.m_height == m_height * other.m_width); }
    bool operator>(DisplayDimension& other) { return (m_width >= other.m_width & m_height >= other.m_height); }
    bool operator==(DisplayDimension& other) {
        return ((m_width == other.m_width) && (m_height == other.m_height) && (m_portrait == other.m_portrait));
    }
    DisplayDimension operator-(DisplayDimension& other) {
        DisplayDimension delta(m_width - other.m_width, m_height - other.m_height);
        return delta;
    }
    void Flip(void) { m_portrait = !m_portrait; }
    bool IsPortrait(void) { return m_portrait; }
    int32_t GetWidth(void) { return m_width; }
    int32_t GetHeight(void) { return m_height; }
    int32_t OrgWidth(void) { return (m_portrait ? m_height : m_width); }
    int32_t OrgHeight(void) { return (m_portrait ? m_width : m_height); }

   private:
    int32_t m_width, m_height;
    bool m_portrait;
};
#endif  // VARTIP_UTIL_H_
