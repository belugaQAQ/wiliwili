#include "platform/android/android_platform.hpp"
#include "platform/android/android_input_mapping.hpp"

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <android/log.h>
#include <android_native_app_glue.h>

#define LOG_TAG "wiliwili"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace brls {

// --- AndroidVideoContext ---

AndroidVideoContext::AndroidVideoContext(ANativeWindow* window)
    : nativeWindow(window) {}

AndroidVideoContext::~AndroidVideoContext() {
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        eglTerminate(display);
    }
}

void AndroidVideoContext::initGL() {
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return;
    }

    if (!eglInitialize(display, nullptr, nullptr)) {
        LOGE("eglInitialize failed");
        return;
    }

    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
        // Fallback to GLES 2.0
        attribs[1] = EGL_OPENGL_ES2_BIT;
        if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
            LOGE("eglChooseConfig failed");
            return;
        }
    }

    EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    context = eglCreateContext(display, config, nullptr, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        // Fallback to GLES 2.0 context
        contextAttribs[1] = 2;
        context = eglCreateContext(display, config, nullptr, contextAttribs);
        if (context == EGL_NO_CONTEXT) {
            LOGE("eglCreateContext failed");
            return;
        }
    }

    recreateSurface();
}

void AndroidVideoContext::recreateSurface() {
    if (surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, surface);
        surface = EGL_NO_SURFACE;
    }

    if (nativeWindow) {
        surface = eglCreateWindowSurface(display, config, nativeWindow, nullptr);
        if (surface == EGL_NO_SURFACE) {
            LOGE("eglCreateWindowSurface failed");
            return;
        }

        if (!eglMakeCurrent(display, surface, surface, context)) {
            LOGE("eglMakeCurrent failed");
            return;
        }

        EGLint w, h;
        eglQuerySurface(display, surface, EGL_WIDTH, &w);
        eglQuerySurface(display, surface, EGL_HEIGHT, &h);
        windowWidth = w;
        windowHeight = h;
        brls::Logger::info("Android EGL surface: {}x{}", w, h);
    }
}

void AndroidVideoContext::setNativeWindow(ANativeWindow* window) {
    nativeWindow = window;
}

void AndroidVideoContext::swapBuffers() {
    if (display != EGL_NO_DISPLAY && surface != EGL_NO_SURFACE) {
        eglSwapBuffers(display, surface);
    }
}

void AndroidVideoContext::clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void AndroidVideoContext::fullScreen(bool fullscreen) {
    // Android TV is always fullscreen
    isFullscreen = true;
}

bool AndroidVideoContext::isFullScreen() {
    return isFullscreen;
}

// --- AndroidInputManager ---

AndroidInputManager::AndroidInputManager() {
    brls::Logger::info("AndroidInputManager initialized");
}

BrlsKeyboardScancode AndroidInputManager::mapAndroidKey(int androidKeyCode) {
    return mapAndroidKeyToBrls(androidKeyCode);
}

void AndroidInputManager::processKeyEvent(int keyCode, int action) {
    BrlsKeyboardScancode key = mapAndroidKey(keyCode);
    if (key == BRLS_KBD_KEY_UNKNOWN) return;

    // Map Android action to press/release
    bool pressed = (action == AKEY_EVENT_ACTION_DOWN || action == AKEY_EVENT_ACTION_MULTIPLE);
    bool released = (action == AKEY_EVENT_ACTION_UP);

    auto* keyboard = Application::getPlatform()->getInputManager()->getKeyboardKeyStateChanged();
    if (keyboard) {
        if (pressed) {
            keyboard->fire(KeyState{key, true, false});
        } else if (released) {
            keyboard->fire(KeyState{key, false, false});
        }
    }
}

void AndroidInputManager::processMotionEvent(float x, float y, int action) {
    // Touch/mouse events (less common on Android TV)
    auto* pointer = Application::getPlatform()->getInputManager()->getPointerStateChanged();
    if (pointer) {
        // Map touch action
        switch (action) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_MOVE:
                pointer->fire(PointerState(x, y, true, 0));
                break;
            case AMOTION_EVENT_ACTION_UP:
                pointer->fire(PointerState(x, y, false, 0));
                break;
        }
    }
}

void AndroidInputManager::updateControllerState() {
    // Controller state is handled through key events on Android
}

// --- AndroidPlatform ---

static AndroidPlatform* g_androidPlatform = nullptr;

AndroidPlatform::AndroidPlatform() {
    g_androidPlatform = this;
    videoContext = new AndroidVideoContext(nullptr);
    inputManager = new AndroidInputManager();
}

AndroidPlatform::~AndroidPlatform() {
    delete videoContext;
    delete inputManager;
    g_androidPlatform = nullptr;
}

AndroidPlatform* AndroidPlatform::instance() {
    return g_androidPlatform;
}

void AndroidPlatform::init() {
    brls::Logger::info("AndroidPlatform initialized");
}

MainLoopResult AndroidPlatform::mainLoop() {
    if (paused) {
        // When paused, sleep to avoid busy loop
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return MainLoopResult::CONTINUE;
    }
    return MainLoopResult::CONTINUE;
}

void AndroidPlatform::createWindow(std::string title) {
    brls::Logger::info("Creating Android window: {}", title);
    if (nativeWindow) {
        videoContext->setNativeWindow((ANativeWindow*)nativeWindow);
        videoContext->initGL();
    }
}

void AndroidPlatform::onPause() {
    paused = true;
    brls::Logger::info("AndroidPlatform: onPause");
    // Notify application about focus loss
    auto* focusEvent = brls::Application::getFocusEvent();
    if (focusEvent) {
        focusEvent->fire(false);
    }
}

void AndroidPlatform::onResume() {
    paused = false;
    brls::Logger::info("AndroidPlatform: onResume");
    auto* focusEvent = brls::Application::getFocusEvent();
    if (focusEvent) {
        focusEvent->fire(true);
    }
}

void AndroidPlatform::disableScreenDimming(bool disable, const std::string& reason, const std::string& pkg) {
    // Android: use FLAG_KEEP_SCREEN_ON via JNI
    brls::Logger::info("disableScreenDimming: {} (reason: {})", disable, reason);
}

void AndroidPlatform::openBrowser(const std::string& url) {
    brls::Logger::info("openBrowser: {} (not implemented on Android)", url);
    // Could use JNI to launch an Intent with ACTION_VIEW
}

std::string AndroidPlatform::getIpAddress() {
    // Use getifaddrs or Android API via JNI
    return "";
}

void AndroidPlatform::exitToHomeMode(bool enable) {
    // On Android, pressing back goes to home by default
}

void AndroidPlatform::setNativeWindow(ANativeWindow* window) {
    nativeWindow = window;
    if (videoContext) {
        videoContext->setNativeWindow(window);
        videoContext->recreateSurface();
    }
}

}  // namespace brls
