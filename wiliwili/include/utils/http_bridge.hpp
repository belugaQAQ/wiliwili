//
// HTTP bridge abstraction for Android (OkHttp via JNI) and other platforms (cpr)
//

#pragma once

#include <string>
#include <map>

namespace bilibili {

class HttpBridge {
public:
    // Synchronous GET request
    // Returns JSON string: {"status_code":200,"url":"...","body":"...","headers":{...},"error":""}
    static std::string get(const std::string& url, const std::map<std::string, std::string>& headers);

    // Synchronous POST request with form data
    // Returns JSON string: {"status_code":200,"url":"...","body":"...","headers":{...},"error":""}
    static std::string post(const std::string& url, const std::map<std::string, std::string>& headers, const std::map<std::string, std::string>& formData);

    // Helper functions
    static std::string mapToJson(const std::map<std::string, std::string>& map);
    static std::string escapeJson(const std::string& s);
};

} // namespace bilibili
