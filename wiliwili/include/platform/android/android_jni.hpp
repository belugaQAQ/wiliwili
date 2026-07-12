#pragma once

#include <jni.h>
#include <string>

namespace brls {

class AndroidJNI {
public:
    static void init(JavaVM* vm, jobject activity);

    static JavaVM* getJavaVM() { return javaVM; }
    static JNIEnv* getJNIEnv();

    // Utility methods
    static void openBrowser(const std::string& url);
    static void keepScreenOn(bool enable);
    static std::string getInternalDataPath();
    static std::string getIpAddress();

private:
    static JavaVM* javaVM;
    static jobject activityRef;
};

}  // namespace brls
