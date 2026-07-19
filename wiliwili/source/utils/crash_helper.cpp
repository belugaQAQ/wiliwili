#include "utils/crash_helper.hpp"
#include <borealis/core/logger.hpp>

#if defined(__ANDROID__)

#include <android/log.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

// Recursively create directories (like mkdir -p). Best-effort; ignores
// errors from already-existing directories.
static void mkdirp(const char* path) {
    char tmp[512];
    std::snprintf(tmp, sizeof(tmp), "%s", path);
    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            ::mkdir(tmp, 0700);
            *p = '/';
        }
    }
    ::mkdir(tmp, 0700);
}

// Return the startup log path. DELIBERATELY does NOT call SDL functions —
// wiliwili_logf() may be called from JNI_OnLoad, which runs during
// System.loadLibrary("wiliwili"), BEFORE SDLActivity.onCreate has
// initialized SDL. Calling SDL_AndroidGetExternalStoragePath() at that
// point crashes (SDL's Java side is not ready). The path is hardcoded
// based on the package name; it is the same external app-specific storage
// path that SDL_AndroidGetExternalStoragePath would return.
static const char* startupLogPath() {
    static const char* path =
        "/sdcard/Android/data/cn.xfangfang.wiliwili/files/wiliwili/wiliwili_startup.log";
    static bool dirCreated = false;
    if (!dirCreated) {
        char dir[512];
        std::snprintf(dir, sizeof(dir), "%s", path);
        char* slash = std::strrchr(dir, '/');
        if (slash) { *slash = '\0'; mkdirp(dir); }
        dirCreated = true;
    }
    return path;
}

// Append a line to the startup log file (best-effort, never throws).
// Used by crash_helper, main.cpp and android_http.cpp to trace the boot
// sequence when logcat is unavailable.
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
        ">>> CRASH: signal %d (%s) <<<  [crash_helper.cpp androidCrashHandler]",
        sig, sigName);
    __android_log_write(ANDROID_LOG_FATAL, "wiliwili", buf);
    // Write to file so we can inspect it even without logcat.
    if (FILE* f = std::fopen(startupLogPath(), "a")) {
        std::fprintf(f, "\n%s\n", buf);
        std::fflush(f);
        std::fclose(f);
    }
    std::_Exit(1);
}

void wiliwili::initCrashDump() {
    signal(SIGSEGV, androidCrashHandler);
    signal(SIGABRT, androidCrashHandler);
    signal(SIGFPE,  androidCrashHandler);
    signal(SIGILL,  androidCrashHandler);
    signal(SIGBUS,  androidCrashHandler);
    signal(SIGTRAP, androidCrashHandler);
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

#else

void wiliwili::initCrashDump() {}

#endif
