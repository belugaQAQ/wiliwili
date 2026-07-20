#pragma once

#include <string>

namespace wiliwili {

void initCrashDump();

/**
 * Initialise the runtime log file:
 *   1. Try to find a removable USB storage path via StorageManager (JNI).
 *      On success, write to <USB>/wiliwili/wiliwili_runtime.log
 *   2. Otherwise fall back to <externalFilesDir>/wiliwili/wiliwili_runtime.log
 *
 * Subscribes to brls::Logger::getLogEvent() so every log line from the entire
 * app lifetime (start → playback → exit) is mirrored to the file.
 * Safe to call from the main thread after ProgramConfig::init() has run.
 *
 * Returns the absolute path of the file actually opened (empty on failure).
 */
std::string initRuntimeLog();

}  // namespace wiliwili
