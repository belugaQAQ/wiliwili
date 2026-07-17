//
// JNI bridge for HTTP requests on Android using OkHttp
//

#ifdef __ANDROID__

#include <jni.h>
#include <string>
#include <functional>
#include <map>
#include <mutex>
#include <condition_variable>
#include <queue>
#include "utils/http_bridge.hpp"

namespace bilibili {

// JNI globals
static JavaVM* g_jvm = nullptr;
static jclass g_httpBridgeClass = nullptr;
static jmethodID g_getMethod = nullptr;
static jmethodID g_postMethod = nullptr;

// Async callback storage
struct AsyncRequest {
    int requestId;
    std::string responseJson;
    bool completed = false;
};

static std::map<int, AsyncRequest*> g_asyncRequests;
static std::mutex g_asyncMutex;
static std::condition_variable g_asyncCv;
static int g_nextRequestId = 1;

// Helper: Get JNIEnv for current thread
JNIEnv* GetJNIEnv() {
    JNIEnv* env = nullptr;
    if (g_jvm) {
        g_jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (!env) {
            g_jvm->AttachCurrentThread(&env, nullptr);
        }
    }
    return env;
}

// Helper: Convert jstring to std::string
std::string JStringToString(JNIEnv* env, jstring jstr) {
    if (!jstr) return "";
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return result;
}

// Initialize JNI bridge (called from Java)
extern "C" JNIEXPORT void JNICALL
Java_cn_xfangfang_wiliwili_HttpBridge_nativeInit(JNIEnv* env, jclass clazz) {
    env->GetJavaVM(&g_jvm);
    g_httpBridgeClass = (jclass)env->NewGlobalRef(clazz);
    g_getMethod = env->GetStaticMethodID(g_httpBridgeClass, "get", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
    g_postMethod = env->GetStaticMethodID(g_httpBridgeClass, "post", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
}

// Async callback from Java
extern "C" JNIEXPORT void JNICALL
Java_cn_xfangfang_wiliwili_HttpBridge_onAsyncResponse(JNIEnv* env, jclass clazz, jint requestId, jstring responseJson) {
    std::lock_guard<std::mutex> lock(g_asyncMutex);
    auto it = g_asyncRequests.find(requestId);
    if (it != g_asyncRequests.end()) {
        it->second->responseJson = JStringToString(env, responseJson);
        it->second->completed = true;
        g_asyncCv.notify_all();
    }
}

// Synchronous GET request
std::string HttpBridge::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    JNIEnv* env = GetJNIEnv();
    if (!env || !g_getMethod) return "";

    // Convert headers to JSON
    std::string headersJson = mapToJson(headers);

    jstring jUrl = env->NewStringUTF(url.c_str());
    jstring jHeaders = env->NewStringUTF(headersJson.c_str());

    jstring jResult = (jstring)env->CallStaticObjectMethod(g_httpBridgeClass, g_getMethod, jUrl, jHeaders);
    std::string result = JStringToString(env, jResult);

    env->DeleteLocalRef(jUrl);
    env->DeleteLocalRef(jHeaders);
    env->DeleteLocalRef(jResult);

    return result;
}

// Synchronous POST request
std::string HttpBridge::post(const std::string& url, const std::map<std::string, std::string>& headers, const std::map<std::string, std::string>& formData) {
    JNIEnv* env = GetJNIEnv();
    if (!env || !g_postMethod) return "";

    std::string headersJson = mapToJson(headers);
    std::string formDataJson = mapToJson(formData);

    jstring jUrl = env->NewStringUTF(url.c_str());
    jstring jHeaders = env->NewStringUTF(headersJson.c_str());
    jstring jFormData = env->NewStringUTF(formDataJson.c_str());

    jstring jResult = (jstring)env->CallStaticObjectMethod(g_httpBridgeClass, g_postMethod, jUrl, jHeaders, jFormData);
    std::string result = JStringToString(env, jResult);

    env->DeleteLocalRef(jUrl);
    env->DeleteLocalRef(jHeaders);
    env->DeleteLocalRef(jFormData);
    env->DeleteLocalRef(jResult);

    return result;
}

// Helper: Convert map to JSON string
std::string HttpBridge::mapToJson(const std::map<std::string, std::string>& map) {
    std::string json = "{";
    bool first = true;
    for (const auto& pair : map) {
        if (!first) json += ",";
        json += "\"" + escapeJson(pair.first) + "\":\"" + escapeJson(pair.second) + "\"";
        first = false;
    }
    json += "}";
    return json;
}

// Helper: Escape JSON string
std::string HttpBridge::escapeJson(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

} // namespace bilibili

#endif // __ANDROID__
