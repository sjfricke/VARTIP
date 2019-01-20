#include "ImageReader.h"
#include <stdlib.h>
#include <string>
#include "Util.h"

// Max buffers in this ImageReader.
#define MAX_BUF_COUNT 2

/*
 * ImageReader listener: called by AImageReader for every frame captured  We pass the event to ImageReader class, so it
 * could do some housekeeping about the loaded queue. For example, we could keep a counter to track how many buffers
 * are full and idle in the queue. If camera almost has no buffer to capture we could release ( skip ) some frames by
 * AImageReader_getNextImage() and AImageReader_delete().
 */
void OnImageCallback(void* ctx, AImageReader* reader) { reinterpret_cast<ImageReader*>(ctx)->ImageCallback(reader); }

// TODO m_imageHeight and m_imageWidth are not used at
ImageReader::ImageReader(ImageFormat* res, enum AIMAGE_FORMATS format)
    : m_reader(nullptr), m_presentRotation(0), m_imageHeight(res->height), m_imageWidth(res->width), m_bufferCount(0) {
    media_status_t status = AImageReader_new(res->width, res->height, format, MAX_BUF_COUNT, &m_reader);
    ASSERT(m_reader && status == AMEDIA_OK, "Failed to create AImageReader");

    AImageReader_ImageListener listener{
        .context = this,
        .onImageAvailable = OnImageCallback,
    };
    AImageReader_setImageListener(m_reader, &listener);

    // assuming 4 bit per pixel max
    LOGE("Image Buffer Size: %d", res->width * res->height * 4);
    m_pImageBuffer = (uint8_t*)malloc(res->width * res->height * 4);
    ASSERT(m_pImageBuffer != nullptr, "Failed to allocate imageBuffer_");
}

ImageReader::~ImageReader() {
    ASSERT(m_reader, "NULL Pointer to %s", __FUNCTION__);
    AImageReader_delete(m_reader);

    if (m_pImageBuffer != nullptr) {
        free(m_pImageBuffer);
    }
}

void ImageReader::ImageCallback(AImageReader* reader) {
    if (m_bufferCount < MAX_BUF_COUNT) {
        m_bufferCount++;
    }
    //  int32_t format;
    //  media_status_t status = AImageReader_getFormat(reader, &format);
    //  ASSERT(status == AMEDIA_OK, "Failed to get the media format");
    //  if (format == AIMAGE_FORMAT_JPEG) {
    //    // Create a thread and write out the jpeg files
    //    AImage *image = nullptr;
    //    media_status_t status = AImageReader_acquireNextImage(reader, &image);
    //    ASSERT(status == AMEDIA_OK && image, "Image is not available");
    //
    //    int planeCount;
    //    status = AImage_getNumberOfPlanes(image, &planeCount);
    //    ASSERT(status == AMEDIA_OK && planeCount == 1,
    //           "Error: getNumberOfPlanes() planceCount = %d", planeCount);
    //    uint8_t *data = nullptr;
    //    int len = 0;
    //    AImage_getPlaneData(image, 0, &data, &len);
    //
    //    AImage_delete(image);
    //  }
}

ANativeWindow* ImageReader::GetNativeWindow(void) {
    if (m_pReader == nullptr) {
        return nullptr;
    }
    ANativeWindow* nativeWindow;
    media_status_t status = AImageReader_getWindow(m_pReader, &nativeWindow);
    ASSERT(status == AMEDIA_OK, "Could not get ANativeWindow");

    return nativeWindow;
}

// Retrieve the next image in ImageReader's bufferQueue, NOT the last image so no image is  skipped
AImage* ImageReader::GetNextImage(void) {
    AImage* image;
    media_status_t status = AImageReader_acquireNextImage(m_pReader, &image);
    if (status != AMEDIA_OK) {
        return nullptr;
    }
    return image;
}

// Retrieve the Last image in ImageReader's bufferQueue, images may be skipped
AImage* ImageReader::GetLatestImage(void) {
    AImage* image;
    media_status_t status = AImageReader_acquireLatestImage(m_pReader, &image);
    if (status != AMEDIA_OK) {
        LOGE("GetLatestImage Status %d", status);
        return nullptr;
    }
    return image;
}

int32_t ImageReader::GetMaxImage(void) {
    int32_t image_count;
    media_status_t status = AImageReader_getMaxImages(m_pReader, &image_count);
    if (status != AMEDIA_OK) {
        return -1;
    }
    return image_count;
}

void ImageReader::DeleteImage(AImage* image) {
    if (image != nullptr) {
        AImage_delete(image);
        image = nullptr;
    }
}

/**
 * Helper function for YUV_420 to RGB conversion. Courtesy of Tensorflow
 * ImageClassifier Sample:
 * https://github.com/tensorflow/tensorflow/blob/master/tensorflow/examples/android/jni/yuv2rgb.cc
 * The difference is that here we have to swap UV plane when calling it.
 */
#ifndef MAX
#define MAX(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a > _b ? _a : _b;      \
    })
#define MIN(a, b)               \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a < _b ? _a : _b;      \
    })
#endif

// This value is 2 ^ 18 - 1, and is used to clamp the RGB values before their
// ranges
// are normalized to eight bits.
static const int kMaxChannelValue = 262143;

static inline uint32_t YUV2RGB(int nY, int nU, int nV) {
    nY -= 16;
    nU -= 128;
    nV -= 128;
    if (nY < 0) nY = 0;

    // This is the floating point equivalent. We do the conversion in integer
    // because some Android devices do not have floating point in hardware.
    // nR = (int)(1.164 * nY + 1.596 * nV);
    // nG = (int)(1.164 * nY - 0.813 * nV - 0.391 * nU);
    // nB = (int)(1.164 * nY + 2.018 * nU);

    int nR = (int)(1192 * nY + 1634 * nV);
    int nG = (int)(1192 * nY - 833 * nV - 400 * nU);
    int nB = (int)(1192 * nY + 2066 * nU);

    nR = MIN(kMaxChannelValue, MAX(0, nR));
    nG = MIN(kMaxChannelValue, MAX(0, nG));
    nB = MIN(kMaxChannelValue, MAX(0, nB));

    nR = (nR >> 10) & 0xff;
    nG = (nG >> 10) & 0xff;
    nB = (nB >> 10) & 0xff;

    return 0xff000000 | (nR << 16) | (nG << 8) | nB;
}

/**
 * Convert yuv image inside AImage into uint32_t
 * uint32_t format is guaranteed to be
 *      WINDOW_FORMAT_RGBX_8888
 *      WINDOW_FORMAT_RGBA_8888
 * @param buf a {@link uint32_t } instance, destination of
 *            image conversion
 * @param image a {@link AImage} instance, source of image conversion.
 *            it will be deleted via {@link AImage_delete}
 */
bool ImageReader::DisplayImage(uint32_t* buf, AImage* image) {
    if (image == nullptr) {
        LOGE("AIamge is soooo null boi");
        return false;
    }

    int32_t srcFormat = -1;
    AImage_getFormat(image, &srcFormat);
    if (AIMAGE_FORMAT_YUV_420_888 != srcFormat) {
        LOGE("Failed to get format");
        return false;
    }
    int32_t srcPlanes = 0;
    AImage_getNumberOfPlanes(image, &srcPlanes);
    ASSERT(srcPlanes == 3, "Is not 3 planes");

    switch (m_presentRotation) {
        case 0:
            PresentImage(buf, image);
            break;
        case 90:
            PresentImage90(buf, image);
            break;
        case 180:
            PresentImage180(buf, image);
            break;
        case 270:
            PresentImage270(buf, image);
            break;
        default:
            ASSERT(0, "NOT recognized display rotation: %d", m_presentRotation);
    }

    AImage_delete(image);
    image = nullptr;

    return true;
}

// Converting yuv to RGB
// No rotation: (x,y) --> (x, y)
// Refer to: https://mathbits.com/MathBits/TISection/Geometry/Transformations2.htm
void ImageReader::PresentImage(uint32_t* buf, AImage* image) {
    AImageCropRect srcRect;
    AImage_getCropRect(image, &srcRect);

    AImage_getPlaneRowStride(image, 0, &m_yStride);
    AImage_getPlaneRowStride(image, 1, &m_uvStride);
    m_pyPixel = m_pImageBuffer;
    AImage_getPlaneData(image, 0, &m_pyPixel, &m_yLen);
    m_pvPixel = m_pImageBuffer + m_yLen;
    AImage_getPlaneData(image, 1, &m_pvPixel, &m_vLen);
    m_puPixel = m_pImageBuffer + m_yLen + m_vLen;
    AImage_getPlaneData(image, 2, &m_puPixel, &m_uLen);
    AImage_getPlanePixelStride(image, 1, &m_uvPixelStride);

    int32_t height = (srcRect.bottom - srcRect.top);
    int32_t width = (srcRect.right - srcRect.left);

    for (int32_t y = 0; y < height; y++) {
        const uint8_t* pY = m_pyPixel + m_yStride * (y + srcRect.top) + srcRect.left;

        int32_t uv_row_start = m_uvStride * ((y + srcRect.top) >> 1);
        const uint8_t* pU = m_puPixel + uv_row_start + (srcRect.left >> 1);
        const uint8_t* pV = m_pvPixel + uv_row_start + (srcRect.left >> 1);

        for (int32_t x = 0; x < width; x++) {
            const int32_t uv_offset = (x >> 1) * m_uvPixelStride;
            buf[x] = YUV2RGB(pY[x], pU[uv_offset], pV[uv_offset]);
        }
        buf += width;
    }
}

//  Converting YUV to RGB
// Rotation image anti-clockwise 90 degree -- (x, y) --> (-y, x)
void ImageReader::PresentImage90(uint32_t* buf, AImage* image) {
    AImageCropRect srcRect;
    AImage_getCropRect(image, &srcRect);

    AImage_getPlaneRowStride(image, 0, &m_yStride);
    AImage_getPlaneRowStride(image, 1, &m_uvStride);
    m_pyPixel = m_pImageBuffer;
    AImage_getPlaneData(image, 0, &m_pyPixel, &m_yLen);
    m_pvPixel = m_pImageBuffer + m_yLen;
    AImage_getPlaneData(image, 1, &m_pvPixel, &m_vLen);
    m_puPixel = m_pImageBuffer + m_yLen + m_vLen;
    AImage_getPlaneData(image, 2, &m_puPixel, &m_uLen);
    AImage_getPlanePixelStride(image, 1, &m_uvPixelStride);

    int32_t height = (srcRect.bottom - srcRect.top);
    int32_t width = (srcRect.right - srcRect.left);

    buf += height - 1;
    for (int32_t y = 0; y < height; y++) {
        const uint8_t* pY = m_pyPixel + m_yStride * (y + srcRect.top) + srcRect.left;

        int32_t uv_row_start = m_uvStride * ((y + srcRect.top) >> 1);
        const uint8_t* pU = m_puPixel + uv_row_start + (srcRect.left >> 1);
        const uint8_t* pV = m_pvPixel + uv_row_start + (srcRect.left >> 1);

        for (int32_t x = 0; x < width; x++) {
            const int32_t uv_offset = (x >> 1) * m_uvPixelStride;
            // [x, y]--> [-y, x]
            int testb = pU[uv_offset];
            int testc = pV[uv_offset];
            int testA = pY[x];
            buf[x * width] = YUV2RGB(testA, testb, testc);
        }
        buf -= 1;  // move to the next column
    }
}

// Converting yuv to RGB
// Rotate image 180 degree: (x, y) --> (-x, -y)
void ImageReader::PresentImage180(uint32_t* buf, AImage* image) {
    AImageCropRect srcRect;
    AImage_getCropRect(image, &srcRect);

    AImage_getPlaneRowStride(image, 0, &m_yStride);
    AImage_getPlaneRowStride(image, 1, &m_uvStride);
    m_pyPixel = m_pImageBuffer;
    AImage_getPlaneData(image, 0, &m_pyPixel, &m_yLen);
    m_pvPixel = m_pImageBuffer + m_yLen;
    AImage_getPlaneData(image, 1, &m_pvPixel, &m_vLen);
    m_puPixel = m_pImageBuffer + m_yLen + m_vLen;
    AImage_getPlaneData(image, 2, &m_puPixel, &m_uLen);
    AImage_getPlanePixelStride(image, 1, &m_uvPixelStride);

    int32_t height = srcRect.bottom - srcRect.top;
    int32_t width = srcRect.right - srcRect.left;

    buf += (height - 1) * width;
    for (int32_t y = 0; y < height; y++) {
        const uint8_t* pY = m_pyPixel + m_yStride * (y + srcRect.top) + srcRect.left;

        int32_t uv_row_start = m_uvStride * ((y + srcRect.top) >> 1);
        const uint8_t* pU = m_puPixel + uv_row_start + (srcRect.left >> 1);
        const uint8_t* pV = m_pvPixel + uv_row_start + (srcRect.left >> 1);

        for (int32_t x = 0; x < width; x++) {
            const int32_t uv_offset = (x >> 1) * m_uvPixelStride;
            // mirror image since we are using front camera
            buf[width - 1 - x] = YUV2RGB(pY[x], pU[uv_offset], pV[uv_offset]);
            // out[x] = YUV2RGB(pY[x], pU[uv_offset], pV[uv_offset]);
        }
        buf -= width;
    }
}

// Converting image from YUV to RGB
// Rotate Image counter-clockwise 270 degree: (x, y) --> (y, x)
void ImageReader::PresentImage270(uint32_t* buf, AImage* image) {
    AImageCropRect srcRect;
    AImage_getCropRect(image, &srcRect);

    AImage_getPlaneRowStride(image, 0, &m_yStride);
    AImage_getPlaneRowStride(image, 1, &m_uvStride);
    m_pyPixel = m_pImageBuffer;
    AImage_getPlaneData(image, 0, &m_pyPixel, &m_yLen);
    m_pvPixel = m_pImageBuffer + m_yLen;
    AImage_getPlaneData(image, 1, &m_pvPixel, &m_vLen);
    m_puPixel = m_pImageBuffer + m_yLen + m_vLen;
    AImage_getPlaneData(image, 2, &m_puPixel, &m_uLen);
    AImage_getPlanePixelStride(image, 1, &m_uvPixelStride);

    int32_t height = srcRect.bottom - srcRect.top;
    int32_t width = srcRect.right - srcRect.left;

    for (int32_t y = 0; y < height; y++) {
        const uint8_t* pY = m_pyPixel + m_yStride * (y + srcRect.top) + srcRect.left;

        int32_t uv_row_start = m_uvStride * ((y + srcRect.top) >> 1);
        const uint8_t* pU = m_puPixel + uv_row_start + (srcRect.left >> 1);
        const uint8_t* pV = m_pvPixel + uv_row_start + (srcRect.left >> 1);

        for (int32_t x = 0; x < width; x++) {
            const int32_t uv_offset = (x >> 1) * m_uvPixelStride;
            int testb = pU[uv_offset];
            int testc = pV[uv_offset];
            int testA = pY[x];
            buf[(width - 1 - x) * width] = YUV2RGB(testA, testb, testc);
        }
        buf += 1;  // move to the next column
    }
}

void ImageReader::SetPresentRotation(int32_t angle) { m_presentRotation = angle; }
