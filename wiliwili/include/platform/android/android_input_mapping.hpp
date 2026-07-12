#pragma once

#include <borealis/core/input.hpp>
#include <android/keycodes.h>

namespace brls {

// Android key event action constants
constexpr int ANDROID_KEY_ACTION_DOWN    = 0;
constexpr int ANDROID_KEY_ACTION_UP      = 1;
constexpr int ANDROID_KEY_ACTION_MULTIPLE = 2;

// Android motion event action constants
constexpr int ANDROID_MOTION_ACTION_DOWN = 0;
constexpr int ANDROID_MOTION_ACTION_UP   = 1;
constexpr int ANDROID_MOTION_ACTION_MOVE = 2;

// Extended key mapping for Android TV
struct AndroidKeyMapping {
    int androidKeyCode;
    BrlsKeyboardScancode brlsKey;
    const char* name;
};

// Primary navigation keys (D-Pad + confirm/back)
static const AndroidKeyMapping NAV_KEYS[] = {
    {AKEYCODE_DPAD_UP,     BRLS_KBD_KEY_UP,        "DPad Up"},
    {AKEYCODE_DPAD_DOWN,   BRLS_KBD_KEY_DOWN,      "DPad Down"},
    {AKEYCODE_DPAD_LEFT,   BRLS_KBD_KEY_LEFT,      "DPad Left"},
    {AKEYCODE_DPAD_RIGHT,  BRLS_KBD_KEY_RIGHT,     "DPad Right"},
    {AKEYCODE_DPAD_CENTER, BRLS_KBD_KEY_ENTER,     "DPad Center"},
    {AKEYCODE_ENTER,       BRLS_KBD_KEY_ENTER,     "Enter"},
    {AKEYCODE_BACK,        BRLS_KBD_KEY_ESCAPE,    "Back"},
    {AKEYCODE_ESCAPE,      BRLS_KBD_KEY_ESCAPE,    "Escape"},
};

// Media control keys
static const AndroidKeyMapping MEDIA_KEYS[] = {
    {AKEYCODE_MEDIA_PLAY_PAUSE,  BRLS_KBD_KEY_SPACE,  "Play/Pause"},
    {AKEYCODE_MEDIA_FAST_FORWARD, BRLS_KBD_KEY_RIGHT,  "Fast Forward"},
    {AKEYCODE_MEDIA_REWIND,      BRLS_KBD_KEY_LEFT,   "Rewind"},
    {AKEYCODE_MEDIA_NEXT,        BRLS_KBD_KEY_N,      "Next"},
    {AKEYCODE_MEDIA_PREVIOUS,    BRLS_KBD_KEY_P,      "Previous"},
    {AKEYCODE_MEDIA_STOP,        BRLS_KBD_KEY_S,      "Stop"},
};

// Gamepad buttons
static const AndroidKeyMapping GAMEPAD_KEYS[] = {
    {AKEYCODE_BUTTON_A,      BRLS_KBD_KEY_ENTER,     "Button A"},
    {AKEYCODE_BUTTON_B,      BRLS_KBD_KEY_ESCAPE,    "Button B"},
    {AKEYCODE_BUTTON_X,      BRLS_KBD_KEY_X,         "Button X"},
    {AKEYCODE_BUTTON_Y,      BRLS_KBD_KEY_Y,         "Button Y"},
    {AKEYCODE_BUTTON_START,  BRLS_KBD_KEY_ENTER,     "Start"},
    {AKEYCODE_BUTTON_SELECT, BRLS_KBD_KEY_TAB,       "Select"},
    {AKEYCODE_BUTTON_L1,     BRLS_KBD_KEY_PAGE_UP,   "L1"},
    {AKEYCODE_BUTTON_R1,     BRLS_KBD_KEY_PAGE_DOWN,  "R1"},
    {AKEYCODE_BUTTON_THUMBL, BRLS_KBD_KEY_TAB,       "Left Thumb"},
    {AKEYCODE_BUTTON_THUMBR, BRLS_KBD_KEY_TAB,       "Right Thumb"},
    {AKEYCODE_BUTTON_MODE,   BRLS_KBD_KEY_TAB,       "Mode"},
};

// Keyboard shortcuts (for Android TV with keyboard attached)
static const AndroidKeyMapping SHORTCUT_KEYS[] = {
    {AKEYCODE_F,           BRLS_KBD_KEY_F,           "F (Fullscreen)"},
    {AKEYCODE_D,           BRLS_KBD_KEY_D,           "D (Danmaku)"},
    {AKEYCODE_Q,           BRLS_KBD_KEY_Q,           "Q (Quality)"},
    {AKEYCODE_S,           BRLS_KBD_KEY_S,           "S (Speed)"},
    {AKEYCODE_SPACE,       BRLS_KBD_KEY_SPACE,       "Space"},
    {AKEYCODE_TAB,         BRLS_KBD_KEY_TAB,         "Tab"},
    {AKEYCODE_VOLUME_UP,   BRLS_KBD_KEY_PAGE_UP,     "Volume Up"},
    {AKEYCODE_VOLUME_DOWN, BRLS_KBD_KEY_PAGE_DOWN,   "Volume Down"},
    {AKEYCODE_MUTE,        BRLS_KBD_KEY_M,           "Mute"},
    {AKEYCODE_INFO,        BRLS_KBD_KEY_I,           "Info"},
    {AKEYCODE_MENU,        BRLS_KBD_KEY_TAB,         "Menu"},
};

// Map an Android keycode to a borealis keyboard scancode
inline BrlsKeyboardScancode mapAndroidKeyToBrls(int androidKeyCode) {
    // Check navigation keys first (most common)
    for (const auto& mapping : NAV_KEYS) {
        if (mapping.androidKeyCode == androidKeyCode) return mapping.brlsKey;
    }
    // Check gamepad keys
    for (const auto& mapping : GAMEPAD_KEYS) {
        if (mapping.androidKeyCode == androidKeyCode) return mapping.brlsKey;
    }
    // Check media keys
    for (const auto& mapping : MEDIA_KEYS) {
        if (mapping.androidKeyCode == androidKeyCode) return mapping.brlsKey;
    }
    // Check shortcut keys
    for (const auto& mapping : SHORTCUT_KEYS) {
        if (mapping.androidKeyCode == androidKeyCode) return mapping.brlsKey;
    }
    // Map letter keys A-Z (excluding already mapped ones)
    if (androidKeyCode >= AKEYCODE_A && androidKeyCode <= AKEYCODE_Z) {
        return static_cast<BrlsKeyboardScancode>(BRLS_KBD_KEY_A + (androidKeyCode - AKEYCODE_A));
    }
    // Map digit keys 0-9
    if (androidKeyCode >= AKEYCODE_0 && androidKeyCode <= AKEYCODE_9) {
        return static_cast<BrlsKeyboardScancode>(BRLS_KBD_KEY_0 + (androidKeyCode - AKEYCODE_0));
    }
    return BRLS_KBD_KEY_UNKNOWN;
}

// Check if a key should be consumed (not passed to Android system)
inline bool shouldConsumeKey(int androidKeyCode) {
    switch (androidKeyCode) {
        case AKEYCODE_DPAD_UP:
        case AKEYCODE_DPAD_DOWN:
        case AKEYCODE_DPAD_LEFT:
        case AKEYCODE_DPAD_RIGHT:
        case AKEYCODE_DPAD_CENTER:
        case AKEYCODE_BACK:
            return true;
        default:
            return false;
    }
}

}  // namespace brls
