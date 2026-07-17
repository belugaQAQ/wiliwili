// Android shim: curl/curl.h
//
// On Android wiliwili no longer links libcurl (the mbedTLS RNG bug made
// every HTTPS call fail with "ssl_init failed"). HTTP is done via OkHttp
// through JNI (see wiliwili/source/android_shim/android_http.cpp and the
// cpr::Session shim in android_shim/cpr/cpr.h).
//
// A few source files still reference curl types directly:
//   - wiliwili/include/api/bilibili/util/http.hpp  (CurlSharedObject)
//   - wiliwili/source/utils/image_helper.cpp        (CURLOPT_SHARE, ...)
// These calls are performance hints (DNS/share handle caching) that have
// no effect when the actual transport is OkHttp, so the symbols below are
// declared as inert no-ops purely to keep those files compiling unchanged.
#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CURL { int _shim_unused; } CURL;
typedef struct CURLSH { int _shim_unused; } CURLSH;

typedef int CURLcode;
typedef int CURLoption;
typedef int CURLSHoption;
typedef int CURLINFO;
/* curl_lock_data / curl_lock_access — used in the lock_callback /
 * unlock_callback signatures in http.hpp's CurlSharedObject. The shim
 * never actually invokes these callbacks (curl_share_setopt is a no-op),
 * but the types must exist so the function pointers compile. */
typedef int curl_lock_data;
typedef int curl_lock_access;

/* CURLoption constants actually referenced by the codebase */
#define CURLOPT_SHARE               ((CURLoption)10000)
#define CURLOPT_DNS_CACHE_TIMEOUT   ((CURLoption)10001)
#define CURLOPT_URL                 ((CURLoption)10002)
#define CURLOPT_SSL_VERIFYPEER      ((CURLoption)10003)
#define CURLOPT_SSL_VERIFYHOST      ((CURLoption)10004)
#define CURLOPT_VERBOSE             ((CURLoption)10005)
#define CURLOPT_STDERR              ((CURLoption)10006)
#define CURLOPT_WRITEDATA           ((CURLoption)10007)
#define CURLOPT_WRITEFUNCTION       ((CURLoption)10008)
#define CURLOPT_CONNECTTIMEOUT      ((CURLoption)10009)
#define CURLOPT_TIMEOUT             ((CURLoption)10010)

/* CURLSHoption */
#define CURLSHOPT_SHARE     ((CURLSHoption)20000)
#define CURLSHOPT_LOCKFUNC  ((CURLSHoption)20001)
#define CURLSHOPT_UNLOCKFUNC ((CURLSHoption)20002)
#define CURLSHOPT_USERDATA  ((CURLSHoption)20003)

/* curl_lock_data */
#define CURL_LOCK_DATA_DNS   5
#define CURL_LOCK_DATA_LAST  10

/* CURLINFO */
#define CURLINFO_OS_ERRNO       ((CURLINFO)30000)
#define CURLINFO_RESPONSE_CODE  ((CURLINFO)30001)

/* CURLcode values */
#define CURLE_OK 0
#define CURLE_SSL_CONNECT_ERROR 35

/* curl_global_init flags (no-ops; OkHttp self-initializes on first use) */
#define CURL_GLOBAL_SSL    (1L << 0)
#define CURL_GLOBAL_WIN32  (1L << 1)
#define CURL_GLOBAL_DEFAULT (CURL_GLOBAL_SSL | CURL_GLOBAL_WIN32)

/* curl_global_init / curl_global_init_mem — no-ops. config_helper.cpp calls
 * curl_global_init(CURL_GLOBAL_DEFAULT) on Android; the PSV branch calls
 * curl_global_init_mem (kept here so the prototype is visible, never used
 * on Android because the GXM branch is compiled out). */
static inline CURLcode curl_global_init(long flags) { (void)flags; return CURLE_OK; }
static inline void curl_global_cleanup(void) {}
static inline CURLcode curl_global_init_mem(long flags,
    void *(*m)(size_t), void (*f)(void*), void *(*r)(void*, size_t),
    char *(*s)(const char*), void *(*c)(size_t, size_t)) {
    (void)flags; (void)m; (void)f; (void)r; (void)s; (void)c;
    return CURLE_OK;
}

/* All curl_easy_* / curl_share_* are no-ops: OkHttp handles DNS caching,
 * connection pooling and TLS internally. */
static inline CURL* curl_easy_init(void) {
    static CURL dummy;
    return &dummy;
}
static inline CURLcode curl_easy_setopt(CURL* curl, CURLoption opt, ...) {
    (void)curl; (void)opt;
    return CURLE_OK;
}
static inline void curl_easy_cleanup(CURL* curl) { (void)curl; }
static inline CURLcode curl_easy_perform(CURL* curl) { (void)curl; return CURLE_OK; }
static inline CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...) {
    (void)curl; (void)info;
    return CURLE_OK;
}
static inline const char* curl_easy_strerror(CURLcode code) {
    (void)code;
    return "curl shim (OkHttp transport)";
}

static inline CURLSH* curl_share_init(void) {
    static CURLSH dummy;
    return &dummy;
}
static inline CURLcode curl_share_setopt(CURLSH* share, CURLSHoption opt, ...) {
    (void)share; (void)opt;
    return CURLE_OK;
}
static inline void curl_share_cleanup(CURLSH* share) { (void)share; }

#ifdef __cplusplus
}
#endif
