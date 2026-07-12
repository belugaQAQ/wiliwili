#include "platform/android/android_jni.hpp"

#include <borealis/core/logger.hpp>

namespace brls {

JavaVM* AndroidJNI::javaVM = nullptr;
jobject AndroidJNI::activityRef = nullptr;

void AndroidJNI::init(JavaVM* vm, jobject activity) {
    javaVM = vm;
    JNIEnv* env = getJNIEnv();
    if (env && activity) {
        activityRef = env->NewGlobalRef(activity);
    }
    brls::Logger::info("AndroidJNI initialized");
}

JNIEnv* AndroidJNI::getJNIEnv() {
    if (!javaVM) return nullptr;

    JNIEnv* env = nullptr;
    bool attached = false;

    int result = javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        result = javaVM->AttachCurrentThread(&env, nullptr);
        attached = (result == JNI_OK);
    }

    return env;
}

void AndroidJNI::openBrowser(const std::string& url) {
    JNIEnv* env = getJNIEnv();
    if (!env || !activityRef) return;

    jclass activityClass = env->GetObjectClass(activityRef);
    if (!activityClass) return;

    jmethodID method = env->GetMethodID(activityClass, "openBrowser", "(Ljava/lang/String;)V");
    if (method) {
        jstring jUrl = env->NewStringUTF(url.c_str());
        env->CallVoidMethod(activityRef, method, jUrl);
        env->DeleteLocalRef(jUrl);
    }

    env->DeleteLocalRef(activityClass);
}

void AndroidJNI::keepScreenOn(bool enable) {
    JNIEnv* env = getJNIEnv();
    if (!env || !activityRef) return;

    jclass activityClass = env->GetObjectClass(activityRef);
    if (!activityClass) return;

    jmethodID method = env->GetMethodID(activityClass, "keepScreenOn", "(Z)V");
    if (method) {
        env->CallVoidMethod(activityRef, method, enable ? JNI_TRUE : JNI_FALSE);
    }

    env->DeleteLocalRef(activityClass);
}

std::string AndroidJNI::getInternalDataPath() {
    JNIEnv* env = getJNIEnv();
    if (!env || !activityRef) return "";

    jclass activityClass = env->GetObjectClass(activityRef);
    if (!activityClass) return "";

    jmethodID method = env->GetMethodID(activityClass, "getFilesDir", "()Ljava/io/File;");
    if (!method) {
        env->DeleteLocalRef(activityClass);
        return "";
    }

    jobject fileObj = env->CallObjectMethod(activityRef, method);
    if (!fileObj) {
        env->DeleteLocalRef(activityClass);
        return "";
    }

    jclass fileClass = env->GetObjectClass(fileObj);
    jmethodID getPath = env->GetMethodID(fileClass, "getAbsolutePath", "()Ljava/lang/String;");
    jstring path = (jstring)env->CallObjectMethod(fileObj, getPath);

    const char* pathStr = env->GetStringUTFChars(path, nullptr);
    std::string result(pathStr);
    env->ReleaseStringUTFChars(path, pathStr);

    env->DeleteLocalRef(path);
    env->DeleteLocalRef(fileClass);
    env->DeleteLocalRef(fileObj);
    env->DeleteLocalRef(activityClass);

    return result;
}

std::string AndroidJNI::getIpAddress() {
    // TODO: implement via NetworkInterface Java API or native getifaddrs
    return "";
}

}  // namespace brls
