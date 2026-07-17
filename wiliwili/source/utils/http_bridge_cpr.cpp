//
// HTTP bridge implementation for non-Android platforms using cpr
//

#ifndef __ANDROID__

#include "utils/http_bridge.hpp"
#include <cpr/cpr.h>

namespace bilibili {

std::string HttpBridge::get(const std::string& url, const std::map<std::string, std::string>& headers) {
    cpr::Header cprHeaders;
    for (const auto& pair : headers) {
        cprHeaders.insert({pair.first, pair.second});
    }

    cpr::Response r = cpr::Get(cpr::Url{url}, cprHeaders, cpr::Timeout{10000});

    // Build response JSON
    std::string json = "{";
    json += "\"status_code\":" + std::to_string(r.status_code);
    json += ",\"url\":\"" + escapeJson(r.url.str()) + "\"";
    json += ",\"body\":\"" + escapeJson(r.text) + "\"";
    json += ",\"headers\":{";
    bool first = true;
    for (const auto& header : r.header) {
        if (!first) json += ",";
        json += "\"" + escapeJson(header.first) + "\":\"" + escapeJson(header.second) + "\"";
        first = false;
    }
    json += "}";
    json += ",\"error\":\"" + escapeJson(r.error.message) + "\"";
    json += "}";

    return json;
}

std::string HttpBridge::post(const std::string& url, const std::map<std::string, std::string>& headers, const std::map<std::string, std::string>& formData) {
    cpr::Header cprHeaders;
    for (const auto& pair : headers) {
        cprHeaders.insert({pair.first, pair.second});
    }

    cpr::Payload payload;
    for (const auto& pair : formData) {
        payload.insert({pair.first, pair.second});
    }

    cpr::Response r = cpr::Post(cpr::Url{url}, cprHeaders, payload, cpr::Timeout{10000});

    // Build response JSON
    std::string json = "{";
    json += "\"status_code\":" + std::to_string(r.status_code);
    json += ",\"url\":\"" + escapeJson(r.url.str()) + "\"";
    json += ",\"body\":\"" + escapeJson(r.text) + "\"";
    json += ",\"headers\":{";
    bool first = true;
    for (const auto& header : r.header) {
        if (!first) json += ",";
        json += "\"" + escapeJson(header.first) + "\":\"" + escapeJson(header.second) + "\"";
        first = false;
    }
    json += "}";
    json += ",\"error\":\"" + escapeJson(r.error.message) + "\"";
    json += "}";

    return json;
}

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

#endif // !__ANDROID__
