// Android shim: cpr/filesystem.h
// cpr ships a backport of <filesystem> for old toolchains. NDK r29 ships a
// fully conformant <filesystem>; just forward to it.
#pragma once
#include <filesystem>
