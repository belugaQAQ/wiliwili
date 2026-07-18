// Android shim: cpr/filesystem.h
//
// cpr ships a backport of <filesystem> for old toolchains and exposes it as
// `cpr::fs`. NDK r29 ships a fully conformant <filesystem>, so we just
// forward to it and re-export it under the cpr namespace (config_helper.cpp
// and shader_helper.cpp use cpr::fs::create_directories / exists /
// directory_iterator / is_directory).
//
// config_helper.cpp includes <cpr/filesystem.h> BEFORE thread_helper.hpp,
// so this is also where CPR_DEFAULT_THREAD_POOL_MAX_THREAD_NUM must be
// defined (real cpr defines it in cpr/thread_pool.hpp, pulled in via
// cpr/cpr.h → cpr/async.h). The value only flows into the shim's no-op
// async::startup, so the exact number is not load-bearing, but we match
// real cpr's expression for fidelity.
#pragma once

#include <filesystem>
#include <thread>

#ifndef CPR_DEFAULT_THREAD_POOL_MAX_THREAD_NUM
#define CPR_DEFAULT_THREAD_POOL_MAX_THREAD_NUM \
    (std::thread::hardware_concurrency() > 2 ? std::thread::hardware_concurrency() : 2)
#endif

namespace cpr {
namespace fs = std::filesystem;
}  // namespace cpr
