#include <android/log.h>
#include <android_native_app_glue.h>
#include "VulkanMain.h"

// Process the next main command.
void handle_cmd(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            InitVulkan(app);
            InitCamera();
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            DeleteVulkan();
            break;
        default:
            __android_log_print(ANDROID_LOG_INFO, "Vulkan Tutorials", "event not handled: %d", cmd);
    }
}

void android_main(struct android_app* app) {
    // Set the callback to process system events
    app->onAppCmd = handle_cmd;

    // Used to poll the events in the main loop
    int events;
    android_poll_source* source;

    // Main loop
    do {
        if (ALooper_pollAll(IsVulkanReady() ? 1 : 0, nullptr, &events, (void**)&source) >= 0) {
            if (source != NULL) source->process(app, source);
        }

        // render if vulkan is ready
        if (IsVulkanReady()) {
            VulkanDrawFrame(app);
        }
    } while (app->destroyRequested == 0);
}
