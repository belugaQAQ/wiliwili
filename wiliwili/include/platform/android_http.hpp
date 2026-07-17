//
// AndroidHttp — low-level JNI bridge to the Java HttpClient (OkHttp).
//
// Used by the cpr::Session shim (android_shim/cpr/cpr.h) to actually
// perform HTTP requests on Android. Only compiled on Android.
//
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bilibili {

struct AndroidHttpResponse {
    int status_code{0};
    std::string text;        // response body (also used as a byte buffer)
    std::string reason;
    std::string error;       // empty when the request succeeded
    std::map<std::string, std::string> header;
    // (name, value) pairs parsed from Set-Cookie response headers.
    std::vector<std::pair<std::string, std::string>> setCookies;
    bool hasError() const { return !error.empty(); }
};

class AndroidHttp {
public:
    static AndroidHttpResponse get(const std::string& url,
                                   const std::map<std::string, std::string>& headers,
                                   const std::string& cookie);

    static AndroidHttpResponse post(const std::string& url,
                                    const std::vector<std::pair<std::string, std::string>>& formParams,
                                    const std::map<std::string, std::string>& headers,
                                    const std::string& cookie);

    // POST a raw body verbatim (analytics JSON, DLNA SOAP XML). The
    // caller's Content-Type header wins; if none is set the body is sent
    // with no content-type.
    static AndroidHttpResponse postRaw(const std::string& url,
                                       const std::string& body,
                                       const std::map<std::string, std::string>& headers,
                                       const std::string& cookie);
};

}  // namespace bilibili
