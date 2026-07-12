#include <android_native_app_glue.h>
#include <android/keycodes.h>

#include "platform/android/android_platform.hpp"
#include "platform/android/android_input_mapping.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>

extern "C" {

static void process_android_input(struct android_app* app, AInputEvent* event) {
    auto* platform = brls::AndroidPlatform::instance();
    if (!platform) return;

    auto* inputManager = dynamic_cast<brls::AndroidInputManager*>(
        platform->getInputManager());
    if (!inputManager) return;

    int type = AInputEvent_getType(event);

    if (type == AINPUT_EVENT_TYPE_KEY) {
        int action = AKeyEvent_getAction(event);
        int keyCode = AKeyEvent_getKeyCode(event);

        // Skip ACTION_MULTIPLE events (repeat handling is done by borealis)
        if (action == ANDROID_KEY_ACTION_MULTIPLE) return;

        inputManager->processKeyEvent(keyCode, action);
    }
    else if (type == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event);
        int actionMasked = action & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);

        inputManager->processMotionEvent(x, y, actionMasked);
    }
}

static int32_t android_input_handler(struct android_app* app, AInputEvent* event) {
    int type = AInputEvent_getType(event);

    if (type == AINPUT_EVENT_TYPE_KEY) {
        int keyCode = AKeyEvent_getKeyCode(event);
        // Always consume D-Pad and Back keys for in-app navigation
        if (brls::shouldConsumeKey(keyCode)) {
            process_android_input(app, event);
            return 1;  // Event consumed
        }
        // Check if we have a mapping for this key
        brls::BrlsKeyboardScancode mapped = brls::mapAndroidKeyToBrls(keyCode);
        if (mapped != brls::BRLS_KBD_KEY_UNKNOWN) {
            process_android_input(app, event);
            return 1;
        }
        return 0;  // Let system handle (e.g. volume keys)
    }
    else if (type == AINPUT_EVENT_TYPE_MOTION) {
        int source = AInputEvent_getSource(event);
        // Handle touch and mouse events
        if (source & AINPUT_SOURCE_TOUCHSCREEN ||
            source & AINPUT_SOURCE_MOUSE) {
            process_android_input(app, event);
            return 1;
        }
        return 0;
    }

    return 0;  // Let system handle
}

static void android_app_cmd(struct android_app* app, int32_t cmd) {
    auto* platform = brls::AndroidPlatform::instance();

    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            brls::Logger::info("Android: APP_CMD_INIT_WINDOW");
            if (app->window && platform) {
                platform->setNativeWindow(app->window);
            }
            break;

        case APP_CMD_TERM_WINDOW:
            brls::Logger::info("Android: APP_CMD_TERM_WINDOW");
            if (platform) {
                platform->setNativeWindow(nullptr);
            }
            break;

        case APP_CMD_PAUSE:
            brls::Logger::info("Android: APP_CMD_PAUSE");
            if (platform) platform->onPause();
            break;

        case APP_CMD_RESUME:
            brls::Logger::info("Android: APP_CMD_RESUME");
            if (platform) platform->onResume();
            break;

        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_LOST_FOCUS:
            brls::Logger::info("Android: Focus changed: {}", cmd);
            break;
    }
}

void android_main(struct android_app* app) {
    brls::Logger::info("android_main: starting wiliwili");

    app->onInputEvent = android_input_handler;
    app->onAppCmd = android_app_cmd;

    // Wait for window to be initialized
    int events;
    struct android_poll_source* source;
    while (!app->window) {
        if (ALooper_pollAll(-1, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(app, source);
        }
        if (app->destroyRequested) return;
    }

    // Now we have a window, proceed with wiliwili initialization
    // The actual main() function will be called by the NativeActivity framework
    // This android_main handles the Android-specific event loop

    brls::Logger::info("android_main: window ready, entering event loop");

    while (!app->destroyRequested) {
        if (ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(app, source);
        }
    }

    brls::Logger::info("android_main: exiting");
}

}  // extern "C"
