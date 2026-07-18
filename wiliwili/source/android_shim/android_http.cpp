//
// AndroidHttp JNI bridge — calls cn.xfangfang.wiliwili.HttpClient (OkHttp).
//
// Only compiled on Android. The JNIEnv is obtained via SDL_AndroidGetJNIEnv
// (SDL2 manages thread attach/detach), so this is safe to call from any
// thread, including cpr's per-request threads and the image thread pool.
//

#include "platform/android_http.hpp"

#include <jni.h>

#include <cstring>

#ifdef __ANDROID__

// SDL2 provides this on Android; it returns the JNIEnv* for the current
// thread (attaching it if necessary). Declared here to avoid pulling in
// SDL_system.h, whose include path is set up by the borealis SDL2 target
// and is not guaranteed to be visible to every wiliwili TU.
extern "C" void* SDL_AndroidGetJNIEnv(void);

namespace bilibili {
namespace {

// Cached class / method / field references. JNI method & field IDs remain
// valid for the lifetime of the class, and a jclass must be promoted to a
// global ref to survive across JNI frames.
struct JniCache {
    jclass httpClientClass{nullptr};
    jmethodID getMethod{nullptr};
    jmethodID postMethod{nullptr};
    jmethodID postRawMethod{nullptr};

    jclass httpResponseClass{nullptr};
    jfieldID codeField{nullptr};
    jfieldID reasonField{nullptr};
    jfieldID bodyField{nullptr};
    jfieldID textField{nullptr};
    jfieldID errorField{nullptr};
    jfieldID setCookiesField{nullptr};

    bool initialized{false};
    bool ok{false};
};

JniCache& cache() {
    static JniCache c;
    return c;
}

bool ensureCache() {
    JniCache& c = cache();
    if (c.initialized) return c.ok;
    c.initialized = true;

    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env) return false;

    // HttpClient class + static methods.
    jclass local = env->FindClass("cn/xfangfang/wiliwili/HttpClient");
    if (!local) return false;
    c.httpClientClass = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);

    c.getMethod = env->GetStaticMethodID(
        c.httpClientClass, "get",
        "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)"
        "Lcn/xfangfang/wiliwili/HttpResponse;");
    c.postMethod = env->GetStaticMethodID(
        c.httpClientClass, "post",
        "(Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;"
        "[Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)"
        "Lcn/xfangfang/wiliwili/HttpResponse;");
    c.postRawMethod = env->GetStaticMethodID(
        c.httpClientClass, "postRaw",
        "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)"
        "Lcn/xfangfang/wiliwili/HttpResponse;");
    if (!c.getMethod || !c.postMethod || !c.postRawMethod) return false;

    // HttpResponse class + instance fields.
    local = env->FindClass("cn/xfangfang/wiliwili/HttpResponse");
    if (!local) return false;
    c.httpResponseClass = static_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);

    c.codeField = env->GetFieldID(c.httpResponseClass, "code", "I");
    c.reasonField = env->GetFieldID(c.httpResponseClass, "reason", "Ljava/lang/String;");
    c.bodyField = env->GetFieldID(c.httpResponseClass, "body", "[B");
    c.textField = env->GetFieldID(c.httpResponseClass, "text", "Ljava/lang/String;");
    c.errorField = env->GetFieldID(c.httpResponseClass, "error", "Ljava/lang/String;");
    c.setCookiesField = env->GetFieldID(c.httpResponseClass, "setCookies", "[Ljava/lang/String;");
    if (!c.codeField || !c.bodyField || !c.textField || !c.errorField) return false;

    c.ok = true;
    return true;
}

std::string jstrToStd(JNIEnv* env, jstring jstr) {
    if (!jstr) return {};
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string out(chars ? chars : "");
    if (chars) env->ReleaseStringUTFChars(jstr, chars);
    return out;
}

// Build a String[] from a vector<string>.
jobjectArray toJStringArray(JNIEnv* env, const std::vector<std::string>& v) {
    jclass strClass = env->FindClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(v.size()), strClass, nullptr);
    for (jsize i = 0; i < static_cast<jsize>(v.size()); ++i) {
        jstring js = env->NewStringUTF(v[i].c_str());
        env->SetObjectArrayElement(arr, i, js);
        env->DeleteLocalRef(js);
    }
    env->DeleteLocalRef(strClass);
    return arr;
}

// Read a String[] field into a vector<string>.
std::vector<std::string> jstrArrayToVec(JNIEnv* env, jobject obj, jfieldID fid) {
    std::vector<std::string> out;
    jobjectArray arr = static_cast<jobjectArray>(env->GetObjectField(obj, fid));
    if (!arr) return out;
    jsize n = env->GetArrayLength(arr);
    out.reserve(static_cast<size_t>(n));
    for (jsize i = 0; i < n; ++i) {
        jstring js = static_cast<jstring>(env->GetObjectArrayElement(arr, i));
        out.push_back(jstrToStd(env, js));
        if (js) env->DeleteLocalRef(js);
    }
    env->DeleteLocalRef(arr);
    return out;
}

// Parse a "name=value; Path=/; ..." Set-Cookie header into (name, value).
std::pair<std::string, std::string> parseSetCookie(const std::string& sc) {
    // Trim leading whitespace.
    size_t start = sc.find_first_not_of(" \t");
    if (start == std::string::npos) return {};
    size_t semi = sc.find(';', start);
    std::string first = sc.substr(start, semi == std::string::npos ? std::string::npos : semi - start);
    size_t eq = first.find('=');
    if (eq == std::string::npos) return {first, ""};
    return {first.substr(0, eq), first.substr(eq + 1)};
}

// Populate AndroidHttpResponse from a returned HttpResponse object.
void readResponse(JNIEnv* env, jobject obj, AndroidHttpResponse& out) {
    JniCache& c = cache();
    out.status_code = env->GetIntField(obj, c.codeField);
    out.reason = jstrToStd(env, static_cast<jstring>(env->GetObjectField(obj, c.reasonField)));
    out.text = jstrToStd(env, static_cast<jstring>(env->GetObjectField(obj, c.textField)));
    out.error = jstrToStd(env, static_cast<jstring>(env->GetObjectField(obj, c.errorField)));

    // Body bytes (binary-safe). Prefer body[] over text so image data
    // round-trips without UTF-8 truncation at a NUL.
    jbyteArray body = static_cast<jbyteArray>(env->GetObjectField(obj, c.bodyField));
    if (body) {
        jsize n = env->GetArrayLength(body);
        if (n > 0) {
            jbyte* bytes = env->GetByteArrayElements(body, nullptr);
            if (bytes) {
                out.text.assign(reinterpret_cast<const char*>(bytes), static_cast<size_t>(n));
                env->ReleaseByteArrayElements(body, bytes, JNI_ABORT);
            }
        }
        env->DeleteLocalRef(body);
    }

    // Set-Cookie values → (name, value) pairs.
    if (c.setCookiesField) {
        for (const auto& sc : jstrArrayToVec(env, obj, c.setCookiesField)) {
            out.setCookies.push_back(parseSetCookie(sc));
        }
    }
}

// Run a static HttpClient method and unpack the result.
AndroidHttpResponse call(JNIEnv* env, jmethodID method, jstring jurl,
                         jobjectArray hKeys, jobjectArray hVals,
                         jobjectArray pKeys, jobjectArray pVals, jstring jcookie) {
    AndroidHttpResponse out;
    jobject result = (pKeys != nullptr)
                         ? env->CallStaticObjectMethod(cache().httpClientClass, method, jurl, pKeys, pVals, hKeys, hVals,
                                                       jcookie)
                         : env->CallStaticObjectMethod(cache().httpClientClass, method, jurl, hKeys, hVals, jcookie);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        out.error = "JNI exception calling HttpClient";
        return out;
    }
    if (!result) {
        out.error = "HttpClient returned null";
        return out;
    }
    readResponse(env, result, out);
    env->DeleteLocalRef(result);
    return out;
}

}  // namespace

AndroidHttpResponse AndroidHttp::get(const std::string& url,
                                     const std::map<std::string, std::string>& headers,
                                     const std::string& cookie) {
    AndroidHttpResponse out;
    if (!ensureCache()) {
        out.error = "JNI cache init failed";
        return out;
    }
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env) {
        out.error = "no JNIEnv";
        return out;
    }

    std::vector<std::string> keys, vals;
    keys.reserve(headers.size());
    vals.reserve(headers.size());
    for (const auto& kv : headers) {
        keys.push_back(kv.first);
        vals.push_back(kv.second);
    }

    jstring jurl = env->NewStringUTF(url.c_str());
    jstring jcookie = env->NewStringUTF(cookie.c_str());
    jobjectArray hKeys = toJStringArray(env, keys);
    jobjectArray hVals = toJStringArray(env, vals);
    out = call(env, cache().getMethod, jurl, hKeys, hVals, nullptr, nullptr, jcookie);
    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(jcookie);
    env->DeleteLocalRef(hKeys);
    env->DeleteLocalRef(hVals);
    return out;
}

AndroidHttpResponse AndroidHttp::post(const std::string& url,
                                      const std::vector<std::pair<std::string, std::string>>& formParams,
                                      const std::map<std::string, std::string>& headers,
                                      const std::string& cookie) {
    AndroidHttpResponse out;
    if (!ensureCache()) {
        out.error = "JNI cache init failed";
        return out;
    }
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env) {
        out.error = "no JNIEnv";
        return out;
    }

    std::vector<std::string> pKeys, pVals;
    pKeys.reserve(formParams.size());
    pVals.reserve(formParams.size());
    for (const auto& kv : formParams) {
        pKeys.push_back(kv.first);
        pVals.push_back(kv.second);
    }
    std::vector<std::string> hKeys, hVals;
    hKeys.reserve(headers.size());
    hVals.reserve(headers.size());
    for (const auto& kv : headers) {
        hKeys.push_back(kv.first);
        hVals.push_back(kv.second);
    }

    jstring jurl = env->NewStringUTF(url.c_str());
    jstring jcookie = env->NewStringUTF(cookie.c_str());
    jobjectArray pKeysArr = toJStringArray(env, pKeys);
    jobjectArray pValsArr = toJStringArray(env, pVals);
    jobjectArray hKeysArr = toJStringArray(env, hKeys);
    jobjectArray hValsArr = toJStringArray(env, hVals);
    out = call(env, cache().postMethod, jurl, hKeysArr, hValsArr, pKeysArr, pValsArr, jcookie);
    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(jcookie);
    env->DeleteLocalRef(pKeysArr);
    env->DeleteLocalRef(pValsArr);
    env->DeleteLocalRef(hKeysArr);
    env->DeleteLocalRef(hValsArr);
    return out;
}

AndroidHttpResponse AndroidHttp::postRaw(const std::string& url,
                                         const std::string& body,
                                         const std::map<std::string, std::string>& headers,
                                         const std::string& cookie) {
    AndroidHttpResponse out;
    if (!ensureCache()) {
        out.error = "JNI cache init failed";
        return out;
    }
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env) {
        out.error = "no JNIEnv";
        return out;
    }

    std::vector<std::string> hKeys, hVals;
    hKeys.reserve(headers.size());
    hVals.reserve(headers.size());
    for (const auto& kv : headers) {
        hKeys.push_back(kv.first);
        hVals.push_back(kv.second);
    }

    jstring jurl = env->NewStringUTF(url.c_str());
    jstring jbody = env->NewStringUTF(body.c_str());
    jstring jcookie = env->NewStringUTF(cookie.c_str());
    jobjectArray hKeysArr = toJStringArray(env, hKeys);
    jobjectArray hValsArr = toJStringArray(env, hVals);

    jobject result = env->CallStaticObjectMethod(
        cache().httpClientClass, cache().postRawMethod, jurl, jbody, hKeysArr, hValsArr, jcookie);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        out.error = "JNI exception calling HttpClient.postRaw";
    } else if (!result) {
        out.error = "HttpClient.postRaw returned null";
    } else {
        readResponse(env, result, out);
        env->DeleteLocalRef(result);
    }

    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(jbody);
    env->DeleteLocalRef(jcookie);
    env->DeleteLocalRef(hKeysArr);
    env->DeleteLocalRef(hValsArr);
    return out;
}

}  // namespace bilibili

#endif  // __ANDROID__
