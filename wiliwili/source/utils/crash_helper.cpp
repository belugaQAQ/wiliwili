#include "utils/crash_helper.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/event.hpp>
#include <chrono>
#include <ctime>
#include <string>

#if defined(__ANDROID__)

#include <android/log.h>
#include <dirent.h>
#include <errno.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// SDL_AndroidGetExternalStoragePath — declared here to avoid pulling in
// SDL_system.h (whose include path is set up by the borealis SDL2 target).
// NOTE: only safe to call AFTER SDL has initialized its Java side
// (SDLActivity.onCreate). Calling it from JNI_OnLoad (which runs during
// System.loadLibrary, before super.onCreate) crashes. So we avoid it
// entirely in wiliwili_logf and use a self-contained path resolution.
extern "C" const char* SDL_AndroidGetExternalStoragePath(void);

// Cached JavaVM pointer, set by JNI_OnLoad. Used to resolve the app's
// external files dir via JNI (Activity.getExternalFilesDir) without
// depending on SDL being initialized.
static JavaVM* g_javaVM = nullptr;

// Set by JNI_OnLoad so we can detect whether the crash happened during
// .so load (before main()).
static bool g_jniOnLoadRunning = false;

// Once resolved (lazily, on first wiliwili_logf call from a context where
// JNI is usable), holds the absolute path to wiliwili_startup.log.
static char g_logPath[512] = {0};

// Resolve the log file path via JNI: Activity.getExternalFilesDir(null)
// + "/wiliwili/wiliwili_startup.log". Returns nullptr if resolution fails
// (e.g. no Activity available yet). Does NOT call SDL. Uses nested ifs
// instead of goto (C++ forbids jumping over variables with constructors).
static const char* resolveLogPathViaJni() {
    if (!g_javaVM) return nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (g_javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (g_javaVM->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
        else return nullptr;
    } else if (!env) {
        return nullptr;
    }

    const char* result = nullptr;
    // ActivityThread.currentApplicationThread → getApplication → getExternalFilesDir
    jclass atClass = env->FindClass("android/app/ActivityThread");
    if (atClass) {
        jmethodID getThread = env->GetStaticMethodID(atClass, "currentApplicationThread", "()Landroid/app/ApplicationThread;");
        if (getThread) {
            jobject thread = env->CallStaticObjectMethod(atClass, getThread);
            if (thread && !env->ExceptionCheck()) {
                jmethodID getApp = env->GetMethodID(env->GetObjectClass(thread), "getApplication", "()Landroid/app/Application;");
                if (getApp) {
                    jobject app = env->CallObjectMethod(thread, getApp);
                    if (app && !env->ExceptionCheck()) {
                        jmethodID getDir = env->GetMethodID(env->GetObjectClass(app), "getExternalFilesDir", "(Ljava/io/File;)Ljava/io/File;");
                        if (getDir) {
                            jobject dir = env->CallObjectMethod(app, getDir, nullptr);
                            if (dir && !env->ExceptionCheck()) {
                                jmethodID getAbsPath = env->GetMethodID(env->GetObjectClass(dir), "getAbsolutePath", "()Ljava/lang/String;");
                                if (getAbsPath) {
                                    jstring jpath = static_cast<jstring>(env->CallObjectMethod(dir, getAbsPath));
                                    if (jpath && !env->ExceptionCheck()) {
                                        const char* chars = env->GetStringUTFChars(jpath, nullptr);
                                        if (chars) {
                                            std::snprintf(g_logPath, sizeof(g_logPath), "%s/wiliwili/wiliwili_startup.log", chars);
                                            env->ReleaseStringUTFChars(jpath, chars);
                                            result = g_logPath;
                                            char dirbuf[512];
                                            std::snprintf(dirbuf, sizeof(dirbuf), "%s", g_logPath);
                                            char* slash = std::strrchr(dirbuf, '/');
                                            if (slash) { *slash = '\0'; ::mkdir(dirbuf, 0700); }
                                        }
                                        env->DeleteLocalRef(jpath);
                                    }
                                }
                                env->DeleteLocalRef(dir);
                            }
                        }
                        env->DeleteLocalRef(app);
                    }
                }
                env->DeleteLocalRef(thread);
            }
        }
        env->DeleteLocalRef(atClass);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();

    if (attached) g_javaVM->DetachCurrentThread();
    return result;
}

// Return the startup log path. Tries (in order):
//   1. Cached path from a previous successful call.
//   2. JNI resolution via ActivityThread (no SDL dependency).
//   3. SDL_AndroidGetExternalStoragePath (only if SDL is ready; checked
//      by whether g_jniOnLoadRunning is false AND we're past JNI_OnLoad).
//   4. Hardcoded fallback under /sdcard.
// NEVER calls SDL during JNI_OnLoad (SDL's Java side isn't ready then).
static const char* startupLogPath() {
    if (g_logPath[0]) return g_logPath;
    // Try JNI path first (works during JNI_OnLoad since it only needs the
    // Application, which exists by the time System.loadLibrary is called).
    const char* p = resolveLogPathViaJni();
    if (p) return p;
    // SDL not ready or JNI failed — use hardcoded fallback.
    std::snprintf(g_logPath, sizeof(g_logPath),
        "/sdcard/Android/data/cn.xfangfang.wiliwili/files/wiliwili/wiliwili_startup.log");
    // Try to create the parent dir.
    char dirbuf[512];
    std::snprintf(dirbuf, sizeof(dirbuf), "%s", g_logPath);
    char* slash = std::strrchr(dirbuf, '/');
    if (slash) { *slash = '\0'; ::mkdir(dirbuf, 0700); }
    return g_logPath;
}

// Append a line to the startup log file (best-effort, never throws).
// Also writes to logcat. Safe to call from JNI_OnLoad and from any thread.
extern "C" void wiliwili_logf(const char* msg) {
    __android_log_write(ANDROID_LOG_INFO, "wiliwili", msg);
    if (FILE* f = std::fopen(startupLogPath(), "a")) {
        std::fprintf(f, "%s\n", msg);
        std::fflush(f);
        std::fclose(f);
    }
}

static void androidCrashHandler(int sig) {
    const char* sigName = "Unknown";
    switch (sig) {
        case SIGSEGV: sigName = "SIGSEGV"; break;
        case SIGABRT: sigName = "SIGABRT"; break;
        case SIGFPE:  sigName = "SIGFPE";  break;
        case SIGILL:  sigName = "SIGILL";  break;
        case SIGBUS:  sigName = "SIGBUS";  break;
        case SIGTRAP: sigName = "SIGTRAP"; break;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        ">>> CRASH: signal %d (%s) <<<  [crash_helper.cpp androidCrashHandler] jniOnLoad=%d",
        sig, sigName, (int)g_jniOnLoadRunning);
    __android_log_write(ANDROID_LOG_FATAL, "wiliwili", buf);
    // Write to file so we can inspect it even without logcat.
    if (FILE* f = std::fopen(startupLogPath(), "a")) {
        std::fprintf(f, "\n%s\n", buf);
        std::fflush(f);
        std::fclose(f);
    }
    std::_Exit(1);
}

// Called by android_http.cpp's JNI_OnLoad to register the JavaVM so
// wiliwili_logf can resolve the log path via JNI without SDL.
extern "C" void wiliwili_set_javavm(JavaVM* vm) {
    g_javaVM = vm;
}

// Called by android_http.cpp's JNI_OnLoad to mark entry/exit.
extern "C" void wiliwili_set_jni_onload_running(bool running) {
    g_jniOnLoadRunning = running;
}

// Register the crash handler. Called from JNI_OnLoad (android_http.cpp)
// so crashes during .so load are caught, and again from ProgramConfig::init
// (wiliwili::initCrashDump) for redundancy. signal() is idempotent.
extern "C" void wiliwili_register_crash_handler() {
    signal(SIGSEGV, androidCrashHandler);
    signal(SIGABRT, androidCrashHandler);
    signal(SIGFPE,  androidCrashHandler);
    signal(SIGILL,  androidCrashHandler);
    signal(SIGBUS,  androidCrashHandler);
    signal(SIGTRAP, androidCrashHandler);
}

void wiliwili::initCrashDump() {
    wiliwili_register_crash_handler();
}

// =============================================================================
// Runtime log: mirror every brls::Logger line to a file in the same directory
// as the startup log (app-specific external storage). No USB, no permissions.
// =============================================================================

static std::FILE* g_runtimeLog = nullptr;
static char g_runtimeLogPath[512] = {0};

// Build a timestamped log line, mirroring brls::Logger's format.
static std::string formatRuntimeLine(std::chrono::system_clock::time_point tp,
                                     brls::LogLevel level, const std::string& msg) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()).count() % 1000;
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tmv{};
    localtime_r(&tt, &tmv);
    const char* tag = "V";
    switch (level) {
        case brls::LogLevel::LOG_ERROR:   tag = "E"; break;
        case brls::LogLevel::LOG_WARNING: tag = "W"; break;
        case brls::LogLevel::LOG_INFO:    tag = "I"; break;
        case brls::LogLevel::LOG_DEBUG:   tag = "D"; break;
        case brls::LogLevel::LOG_VERBOSE: tag = "V"; break;
    }
    char header[64];
    std::snprintf(header, sizeof(header), "%02d:%02d:%02d.%03d %s ",
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (int)ms, tag);
    return std::string(header) + msg + "\n";
}

std::string wiliwili::initRuntimeLog() {
    if (g_runtimeLog) return g_runtimeLogPath;

    // Reuse the same directory as wiliwili_startup.log. startupLogPath()
    // already resolves the app-specific external storage dir via JNI (or
    // SDL) and caches it in g_logPath. We just swap the filename.
    const char* startup = startupLogPath();
    if (!startup || !g_logPath[0]) {
        __android_log_print(ANDROID_LOG_ERROR, "wiliwili",
            "[runtime_log] startupLogPath unavailable");
        return "";
    }

    // g_logPath = "<dir>/wiliwili_startup.log"  ->  "<dir>/wiliwili_runtime.log"
    char dirbuf[512];
    std::snprintf(dirbuf, sizeof(dirbuf), "%s", g_logPath);
    char* slash = std::strrchr(dirbuf, '/');
    if (!slash) {
        __android_log_print(ANDROID_LOG_ERROR, "wiliwili",
            "[runtime_log] bad g_logPath: '%s'", g_logPath);
        return "";
    }
    *slash = '\0';
    std::snprintf(g_runtimeLogPath, sizeof(g_runtimeLogPath),
                  "%s/wiliwili_runtime.log", dirbuf);

    g_runtimeLog = std::fopen(g_runtimeLogPath, "w");
    if (!g_runtimeLog) {
        __android_log_print(ANDROID_LOG_ERROR, "wiliwili",
            "[runtime_log] fopen('%s', \"w\") FAILED errno=%d", g_runtimeLogPath, errno);
        g_runtimeLogPath[0] = '\0';
        return "";
    }

    // Line-buffer so the file is readable live via `cat` while the app runs.
    std::setvbuf(g_runtimeLog, nullptr, _IOLBF, 0);

    // Subscribe to brls::Logger so every log line gets mirrored to the file.
    // The subscription is global (never released) — runs for app lifetime.
    brls::Logger::getLogEvent()->subscribe(
        [](std::chrono::system_clock::time_point tp, brls::LogLevel level,
           const std::string& msg) {
            if (!g_runtimeLog) return;
            std::string line = formatRuntimeLine(tp, level, msg);
            std::fwrite(line.data(), 1, line.size(), g_runtimeLog);
        });

    __android_log_print(ANDROID_LOG_INFO, "wiliwili",
        "[runtime_log] SUCCESS — log file: %s", g_runtimeLogPath);

    brls::Logger::info("========================================");
    brls::Logger::info("wiliwili runtime log started");
    brls::Logger::info("log file: {}", g_runtimeLogPath);
    brls::Logger::info("========================================");

    return g_runtimeLogPath;
}

#elif defined(_WIN32) && !defined(__WINRT__)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>

LONG WINAPI createMiniDump(_EXCEPTION_POINTERS* pep) {
    MEMORY_BASIC_INFORMATION memInfo;
    PEXCEPTION_RECORD xcpt = pep->ExceptionRecord;
    HANDLE hProcess        = ::GetCurrentProcess();
    HANDLE hThread         = ::GetCurrentThread();
    brls::Logger::error("Exception code {:#x}", xcpt->ExceptionCode);

    if (::VirtualQuery(xcpt->ExceptionAddress, &memInfo, sizeof(memInfo))) {
        char modulePath[MAX_PATH] = "Unknown Module";
        HMODULE hModule           = (HMODULE)memInfo.AllocationBase;
        if (!memInfo.AllocationBase) hModule = GetModuleHandleA(nullptr);
        ::GetModuleFileNameA(hModule, modulePath, sizeof(modulePath));
        brls::Logger::error("Fault address {} {}", fmt::ptr(xcpt->ExceptionAddress), modulePath);
    }

    // Get handles to kernel32 and dbghelp
    HMODULE dbghelp = ::LoadLibraryA("dbghelp.dll");
    if (!dbghelp) return EXCEPTION_CONTINUE_SEARCH;

    auto fnMiniDumpWriteDump = (BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
        CONST PMINIDUMP_EXCEPTION_INFORMATION, CONST PMINIDUMP_USER_STREAM_INFORMATION,
        CONST PMINIDUMP_CALLBACK_INFORMATION))::GetProcAddress(dbghelp, "MiniDumpWriteDump");
    auto fnSymInitialize = (BOOL(WINAPI*)(HANDLE, PCSTR, BOOL))::GetProcAddress(dbghelp, "SymInitialize");
    auto fnSymCleanup = (BOOL(WINAPI*)(HANDLE))::GetProcAddress(dbghelp, "SymCleanup");
    auto fnSymSetOptions = (DWORD(WINAPI*)(DWORD))::GetProcAddress(dbghelp, "SymSetOptions");
    auto fnStackWalk64 = (BOOL(WINAPI*)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64,
        PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64))::GetProcAddress(dbghelp, "StackWalk64");
    auto fnSymGetSymFromAddr64 = (BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64))::GetProcAddress(dbghelp, "SymGetSymFromAddr64");
    auto fnSymGetLineFromAddr64 = (BOOL(WINAPI*)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64))::GetProcAddress(dbghelp, "SymGetLineFromAddr64");
    auto fnSymGetModuleInfo64 = (BOOL(WINAPI*)(HANDLE, DWORD64, PIMAGEHLP_MODULE64))::GetProcAddress(dbghelp, "SymGetModuleInfo64");
    auto fnSymFTA64 = (PFUNCTION_TABLE_ACCESS_ROUTINE64)::GetProcAddress(dbghelp, "SymFunctionTableAccess64");
    auto fnSymGMB64 = (PGET_MODULE_BASE_ROUTINE64)::GetProcAddress(dbghelp, "SymGetModuleBase64");

    STACKFRAME64 s;
    DWORD imageType;
    CONTEXT ctx = *pep->ContextRecord;

    ZeroMemory(&s, sizeof(s));
    s.AddrPC.Mode    = AddrModeFlat;
    s.AddrFrame.Mode = AddrModeFlat;
    s.AddrStack.Mode = AddrModeFlat;
#ifdef _M_IX86
    imageType          = IMAGE_FILE_MACHINE_I386;
    s.AddrPC.Offset    = ctx.Eip;
    s.AddrFrame.Offset = ctx.Ebp;
    s.AddrStack.Offset = ctx.Esp;
#elif _M_X64
    imageType          = IMAGE_FILE_MACHINE_AMD64;
    s.AddrPC.Offset    = ctx.Rip;
    s.AddrFrame.Offset = ctx.Rsp;
    s.AddrStack.Offset = ctx.Rsp;
#elif _M_ARM64
    imageType          = IMAGE_FILE_MACHINE_ARM64;
    s.AddrPC.Offset    = ctx.Pc;
    s.AddrFrame.Offset = ctx.Fp;
    s.AddrStack.Offset = ctx.Sp;
#else
#error "Platform not supported!"
#endif

    std::vector<char> buf(sizeof(IMAGEHLP_SYMBOL64) + MAX_SYM_NAME);
    PIMAGEHLP_SYMBOL64 symbol = reinterpret_cast<PIMAGEHLP_SYMBOL64>(buf.data());
    symbol->SizeOfStruct      = sizeof(IMAGEHLP_SYMBOL64);
    symbol->MaxNameLength     = MAX_SYM_NAME;
    int n                     = 0;

    if (!fnSymInitialize(hProcess, nullptr, TRUE)) {
        brls::Logger::warning("SymInitialize failed {}", ::GetLastError());
    }
    fnSymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    while (fnStackWalk64(imageType, hProcess, hThread, &s, &ctx, nullptr, fnSymFTA64, fnSymGMB64, nullptr)) {
        IMAGEHLP_LINE64 line = {sizeof(IMAGEHLP_LINE64)};
        IMAGEHLP_MODULE64 mi = {sizeof(IMAGEHLP_MODULE64)};
        DWORD64 funcOffset   = 0;
        DWORD lineOffset     = 0;

        if (!fnSymGetModuleInfo64(hProcess, s.AddrPC.Offset, &mi)) {
            strcpy(mi.ImageName, "???");
        }
        if (fnSymGetLineFromAddr64(hProcess, s.AddrPC.Offset, &lineOffset, &line)) {
            brls::Logger::error("[{}]: {}({})", n, line.FileName, line.LineNumber);
        } else if (fnSymGetSymFromAddr64(hProcess, s.AddrPC.Offset, &funcOffset, symbol)) {
            brls::Logger::error("[{}]: {}!{} + {:#x}", n, mi.ImageName, symbol->Name, funcOffset);
        } else {
            brls::Logger::error("[{}]: {} + {:#x}", n, mi.ImageName, s.AddrPC.Offset);
        }
        n++;
    }
    fnSymCleanup(hProcess);

    SYSTEMTIME lt;
    CHAR tempPath[MAX_PATH];
    ::GetLocalTime(&lt);
    snprintf(tempPath, sizeof(tempPath), "crash-%04d%02d%02d-%02d%02d%02d.dmp", 
        lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, lt.wSecond);
    HANDLE hDump = ::CreateFileA(tempPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    MINIDUMP_EXCEPTION_INFORMATION info;
    info.ThreadId          = ::GetCurrentThreadId();
    info.ExceptionPointers = pep;
    info.ClientPointers    = TRUE;
    fnMiniDumpWriteDump(hProcess, ::GetCurrentProcessId(), hDump, MiniDumpNormal, &info, nullptr, nullptr);
    ::CloseHandle(hDump);

    return EXCEPTION_CONTINUE_SEARCH;
}

void wiliwili::initCrashDump() { ::SetUnhandledExceptionFilter(createMiniDump); }

std::string wiliwili::initRuntimeLog() { return ""; }

#else

void wiliwili::initCrashDump() {}

std::string wiliwili::initRuntimeLog() { return ""; }

#endif
