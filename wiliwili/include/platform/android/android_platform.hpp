#pragma once

#include <borealis/core/platform.hpp>
#include <borealis/core/input.hpp>
#include <EGL/egl.h>
#include <android/native_window.h>

#include "platform/android/android_input_mapping.hpp"

namespace brls {

class AndroidVideoContext : public VideoContext {
public:
    AndroidVideoContext(ANativeWindow* window);
    ~AndroidVideoContext() override;

    void initGL() override;
    void swapBuffers() override;
    void clear() override;
    void fullScreen(bool fullscreen) override;
    bool isFullScreen() override;
    EGLDisplay getEGLDisplay() { return display; }
    EGLSurface getEGLSurface() { return surface; }
    EGLContext getEGLContext() { return context; }

    void setNativeWindow(ANativeWindow* window);
    void recreateSurface();

private:
    ANativeWindow* nativeWindow = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = nullptr;
    int windowWidth = 1920;
    int windowHeight = 1080;
    bool isFullscreen = true;  // Android TV is always fullscreen
};

class AndroidInputManager : public InputManager {
public:
    AndroidInputManager();

    void processKeyEvent(int keyCode, int action);
    void processMotionEvent(float x, float y, int action);
    void updateControllerState();

private:
    // Map Android keycodes to borealis key codes (delegates to android_input_mapping.hpp)
    BrlsKeyboardScancode mapAndroidKey(int androidKeyCode);
};

class AndroidPlatform : public Platform {
public:
    AndroidPlatform();
    ~AndroidPlatform() override;

    void init() override;
    MainLoopResult mainLoop() override;
    void createWindow(std::string title) override;
    bool isApplicationMode() override { return true; }

    // Lifecycle
    void onPause();
    void onResume();

    // Platform features
    void disableScreenDimming(bool disable, const std::string& reason, const std::string& pkg) override;
    void openBrowser(const std::string& url) override;
    std::string getIpAddress() override;
    void exitToHomeMode(bool enable) override;

    VideoContext* getVideoContext() override { return videoContext; }
    InputManager* getInputManager() override { return inputManager; }

    void setNativeWindow(ANativeWindow* window);
    void setNativeActivity(void* activity) { nativeActivity = activity; }

    // JNI helpers
    static AndroidPlatform* instance();

private:
    AndroidVideoContext* videoContext = nullptr;
    AndroidInputManager* inputManager = nullptr;
    void* nativeActivity = nullptr;
    bool paused = false;
};

}  // namespace brls
