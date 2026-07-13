cmake_minimum_required(VERSION 3.15)

# add cpr
if (USE_SYSTEM_CPR)
    find_package(cpr CONFIG REQUIRED)
    if (cpr_FOUND)
        message(STATUS "Found cpr: ${cpr_CONFIG} (found version ${cpr_VERSION})")
    endif ()
else ()
    if (USE_SYSTEM_CURL)
        set(CPR_USE_SYSTEM_CURL ON)
    else ()
        set(CPR_USE_SYSTEM_CURL OFF)
    endif ()

    set(BUILD_SHARED_LIBS OFF)
    set(CPR_ENABLE_SSL ON)
    if (PLATFORM_DESKTOP)
        if (WIN32)
            set(CPR_FORCE_WINSSL_BACKEND ON)
        endif ()
    elseif (PLATFORM_IOS)
        set(CPR_USE_SYSTEM_CURL OFF)
        set(CPR_SKIP_CA_BUNDLE_SEARCH ON)
    elseif (PLATFORM_SWITCH OR PLATFORM_PS4 OR PLATFORM_PSV)
        set(CPR_USE_SYSTEM_CURL ON)
        set(CPR_FORCE_MBEDTLS_BACKEND ON)
    elseif (PLATFORM_ANDROID)
        # buildscripts does not build libcurl for Android, so use cpr's
        # built-in curl (FetchContent downloads curl 8.4.0) with the
        # MbedTLS backend (built by buildscripts and installed to prefix).
        # MbedTLS only supports CURLOPT_CAINFO (single bundle file), not
        # CURLOPT_CAPATH (hashed cert directory). Android has no standard
        # CA bundle file, so skip the search and default TLS_VERIFY off
        # (see config_helper.cpp); users who ship a bundle can re-enable it.
        set(CPR_USE_SYSTEM_CURL OFF)
        set(CPR_FORCE_MBEDTLS_BACKEND ON)
        set(CPR_SKIP_CA_BUNDLE_SEARCH ON)

        # Pre-locate MbedTLS and export the exact paths to cpr/curl.
        # cpr's FindMbedTLS.cmake runs find_path/find_library, which under
        # the Android NDK toolchain only search CMAKE_FIND_ROOT_PATH (set
        # to the buildscripts prefix in the parent CMakeLists.txt). That
        # *should* work, but cpr passes the discovered vars down to curl's
        # FetchContent target, and the NDK's cross-root remapping can drop
        # them. Set them explicitly here so both cpr and the fetched curl
        # pick up the same headers and libraries — otherwise curl silently
        # builds without a working TLS backend and every HTTPS call dies
        # at runtime with "mbedTLS: ssl_init failed".
        if (NOT MBEDTLS_FOUND)
            find_path(MBEDTLS_INCLUDE_DIRS mbedtls/ssl.h)
            find_library(MBEDTLS_LIBRARY mbedtls)
            find_library(MBEDX509_LIBRARY mbedx509)
            find_library(MBEDCRYPTO_LIBRARY mbedcrypto)
            if (MBEDTLS_INCLUDE_DIRS AND MBEDTLS_LIBRARY AND MBEDX509_LIBRARY AND MBEDCRYPTO_LIBRARY)
                set(MBEDTLS_LIBRARIES "${MBEDTLS_LIBRARY}" "${MBEDX509_LIBRARY}" "${MBEDCRYPTO_LIBRARY}")
                set(MBEDTLS_FOUND TRUE CACHE BOOL "" FORCE)
                message(STATUS "MbedTLS (pre-located for cpr/curl):")
                message(STATUS "  includes: ${MBEDTLS_INCLUDE_DIRS}")
                message(STATUS "  libs: ${MBEDTLS_LIBRARIES}")
            else ()
                message(FATAL_ERROR "MbedTLS not found in prefix — buildscripts must build mbedtls first")
            endif ()
        endif ()
    endif ()
    add_subdirectory(cpr EXCLUDE_FROM_ALL)
endif ()
if (PLATFORM_PS4)
    list(APPEND APP_PLATFORM_LIB SceNet mbedcrypto mbedtls mbedx509)
    set(APP_PLATFORM_LIB ${APP_PLATFORM_LIB} PARENT_SCOPE)
endif ()
if (PLATFORM_ANDROID)
    list(APPEND APP_PLATFORM_LIB mbedcrypto mbedtls mbedx509 log)
    set(APP_PLATFORM_LIB ${APP_PLATFORM_LIB} PARENT_SCOPE)
endif ()

# add libopencc
if (NOT USE_LIBROMFS AND NOT DISABLE_OPENCC AND NOT PLATFORM_IOS)
    set(BRLS_USE_OPENCC ON)
    if (USE_SYSTEM_OPENCC)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(opencc REQUIRED IMPORTED_TARGET opencc)
        add_library(libopencc ALIAS PkgConfig::opencc)
        set(BRLS_OPENCC_INCLUDE ${opencc_INCLUDE_DIRS})
    else ()
        set(BUILD_SHARED_LIBS OFF)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        add_subdirectory(OpenCC EXCLUDE_FROM_ALL)
        target_compile_options(libopencc PRIVATE -w)
        set(BRLS_OPENCC_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/OpenCC/src)
    endif ()
else ()
    set(BRLS_USE_OPENCC OFF)
endif ()

# add QR-Code-generator
if (USE_SYSTEM_QRCODEGEN)
    find_package(qrcodegencpp REQUIRED GLOBAL)
    add_library(qrcode ALIAS qrcodegencpp::qrcodegencpp)
else ()
    add_library(qrcode STATIC ${CMAKE_CURRENT_SOURCE_DIR}/QR-Code-generator/cpp/qrcodegen.cpp)
    target_include_directories(qrcode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/QR-Code-generator/cpp)
    set_property(TARGET qrcode PROPERTY CXX_STANDARD 17)
endif ()

# add pystring
if (USE_SYSTEM_PYSTRING)
    find_package(PkgConfig)
    if (PkgConfig_FOUND)
        pkg_check_modules(pystring IMPORTED_TARGET pystring)
    endif ()
    if (pystring_FOUND)
        add_library(pystring ALIAS PkgConfig::pystring)
    else ()
        find_path(PYSTRING_INCLUDE_DIR pystring.h PATH_SUFFIXES pystring REQUIRED)
        find_library(PYSTRING_LIBRARY NAMES pystring REQUIRED)
        list(APPEND APP_PLATFORM_INCLUDE ${PYSTRING_INCLUDE_DIR})
        list(APPEND APP_PLATFORM_LIB ${PYSTRING_LIBRARY})
        set(APP_PLATFORM_LIB ${APP_PLATFORM_LIB} PARENT_SCOPE)
        set(APP_PLATFORM_INCLUDE ${APP_PLATFORM_INCLUDE} PARENT_SCOPE)
    endif ()
else ()
    add_library(pystring STATIC ${CMAKE_CURRENT_SOURCE_DIR}/pystring/pystring.cpp)
    target_include_directories(pystring PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pystring)
    set_property(TARGET pystring PROPERTY CXX_STANDARD 17)
endif ()

# add lunasvg
if (USE_SYSTEM_LUNASVG)
    find_package(lunasvg REQUIRED GLOBAL)
    add_library(lunasvg ALIAS lunasvg::lunasvg)
else ()
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    add_subdirectory(lunasvg EXCLUDE_FROM_ALL)
endif ()

# add mongoose
if (USE_SYSTEM_MONGOOSE)
    find_library(MONGOOSE_LIBRARY NAMES mongoose REQUIRED)
    add_library(mongoose UNKNOWN IMPORTED)
    set_property(TARGET mongoose PROPERTY IMPORTED_LOCATION ${MONGOOSE_LIBRARY})
else ()
    set(MG_ENABLE_IPV6 ON)
    add_subdirectory(mongoose EXCLUDE_FROM_ALL)
endif ()

# add MemoryModule
if (MPV_BUNDLE_DLL)
    add_library (MemoryModule STATIC ${CMAKE_CURRENT_SOURCE_DIR}/MemoryModule/MemoryModule.c)
    target_include_directories(MemoryModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/MemoryModule)
endif ()

# add borealis
add_subdirectory(borealis/library)

# add libpdr (不使用 pdr 项目内的 cmake 配置，因为其内的 mongoose 不支持 psv/switch)
file(GLOB_RECURSE PDR_SRC ${CMAKE_CURRENT_SOURCE_DIR}/libpdr/src/*.cpp)
add_library(pdr ${PDR_SRC})
set_property(TARGET pdr PROPERTY CXX_STANDARD 17)
target_include_directories(pdr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libpdr/include)
target_link_libraries(pdr mongoose tinyxml2)

add_library(wiliwiliLibExtra INTERFACE)
target_link_libraries(wiliwiliLibExtra INTERFACE cpr::cpr qrcode)
