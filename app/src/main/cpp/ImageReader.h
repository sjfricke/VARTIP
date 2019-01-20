#ifndef VARTIP_IMAGEREADER_H_
#define VARTIP_IMAGEREADER_H_
#include <media/NdkImageReader.h>
#include "Util.h"

class ImageReader {
   public:
    explicit ImageReader(ImageFormat* res, enum AIMAGE_FORMATS format);

    ~ImageReader();

    /**
     * Report cached ANativeWindow, which was used to create camera's capture
     * session output.
     */
    ANativeWindow* GetNativeWindow(void);

    /**
     * Retrieve Image on the top of Reader's queue
     */
    AImage* GetNextImage(void);

    /**
     * Retrieve Image on the bottom of Reader's queue
     */
    AImage* GetLatestImage(void);

    int32_t GetMaxImage(void);

    /**
     * Delete Image
     * @param image {@link AImage} instance to be deleted
     */
    void DeleteImage(AImage* image);

    /**
     * AImageReader callback handler. Called by AImageReader when a frame is
     * captured
     * (Internal function, not to be called by clients)
     */
    void ImageCallback(AImageReader* reader);

    /**
     * DisplayImage()
     *   Present camera image to the given display buffer. Avaliable image is
     * converted
     *   to display buffer format. Supported display format:
     *      WINDOW_FORMAT_RGBX_8888
     *      WINDOW_FORMAT_RGBA_8888
     *   @param buf {@link uint32_t} for image to display to.
     *   @param image a {@link AImage} instance, source of image conversion.
     *            it will be deleted via {@link AImage_delete}
     *   @return true on success, false on failure
     */
    bool DisplayImage(uint32_t* buf, AImage* image);

    /**
     * Configure the rotation angle necessary to apply to
     * Camera image when presenting: all rotations should be accumulated:
     *    CameraSensorOrientation + Android Device Native Orientation +
     *    Human Rotation (rotated degree related to Phone native orientation
     */
    void SetPresentRotation(int32_t angle);

    uint8_t GetBufferCount() { return buffer_count; }
    void DecBufferCount() { buffer_count--; }
    // void SetImageVk(_vkCallback onImageVk) { m_onImageVk = onImageVk; }

   private:
    //_vkCallback m_onImageVk;

    int32_t presentRotation_;
    AImageReader* reader_;

    void PresentImage(uint32_t* buf, AImage* image);
    void PresentImage90(uint32_t* buf, AImage* image);
    void PresentImage180(uint32_t* buf, AImage* image);
    void PresentImage270(uint32_t* buf, AImage* image);

    void WriteFile(AImage* image);

    int32_t imageHeight_;
    int32_t imageWidth_;

    uint8_t* imageBuffer_;

    int32_t yStride, uvStride;
    uint8_t *yPixel, *uPixel, *vPixel;
    int32_t yLen, uLen, vLen;
    int32_t uvPixelStride;

    uint8_t buffer_count;
};

#endif  // VARTIP_IMAGEREADER_H_
