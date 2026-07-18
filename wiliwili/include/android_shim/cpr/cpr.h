// Android shim: cpr/cpr.h
//
// Drop-in replacement for the cpr (C++ Requests) public API used by
// wiliwili, backed by OkHttp over JNI instead of libcurl. On Android,
// libcurl's mbedTLS backend cannot initialize its RNG (-0x7400), so
// every HTTPS request failed with "ssl_init failed". OkHttp uses
// Android's platform TLS whose RNG works.
//
// This header mirrors ONLY the cpr API surface actually consumed by the
// wiliwili codebase. All data-container types (Url, Parameters, Payload,
// Cookies, Header, Response, ...) are defined inline here; Session,
// MultiPerform and ThreadPool are declared here and implemented in
// wiliwili/source/android_shim/cpr_session.cpp (so the JNI dependency
// stays local to one translation unit).
//
// The shim directory is added to the include path ONLY on Android (see
// CMakeLists.txt), and cpr/curl are no longer built on Android, so these
// headers cleanly shadow the real cpr/curl for the Android build without
// affecting any other platform.
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <curl/curl.h>  // curl shim (no-ops) for CurlSharedObject / image_helper

namespace cpr {

// ---------------------------------------------------------------------------
// URL encoding (cpr::util::urlEncode is used directly by share_dialog.cpp)
// ---------------------------------------------------------------------------
namespace util {
inline std::string urlEncode(const std::string& s) {
    static const char* HEX = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(HEX[(c >> 4) & 0xF]);
            out.push_back(HEX[c & 0xF]);
        }
    }
    return out;
}
}  // namespace util

// ---------------------------------------------------------------------------
// CurlHolder — real cpr exposes GetCurlHolder(); image_helper and
// createSession() reach for ->handle (CURL*). On Android the handle is an
// inert dummy; curl_easy_setopt on it is a no-op (see curl/curl.h).
// ---------------------------------------------------------------------------
class CurlHolder {
public:
    CurlHolder() = default;
    CURL* handle{nullptr};
};

// ---------------------------------------------------------------------------
// Url
// ---------------------------------------------------------------------------
class Url {
public:
    Url() = default;
    Url(const char* s) : str_(s ? s : "") {}
    Url(const std::string& s) : str_(s) {}
    Url(std::string&& s) : str_(std::move(s)) {}
    const std::string& str() const { return str_; }
    operator std::string() const { return str_; }
private:
    std::string str_;
};

using Header = std::map<std::string, std::string>;

// ---------------------------------------------------------------------------
// Timeout / ConnectTimeout
// ---------------------------------------------------------------------------
class Timeout {
public:
    Timeout() = default;
    Timeout(std::int64_t ms) : ms_(ms) {}
    Timeout(const std::chrono::milliseconds& d) : ms_(static_cast<std::int64_t>(d.count())) {}
    std::int64_t ms() const { return ms_; }
private:
    std::int64_t ms_{0};
};

class ConnectTimeout : public Timeout {
public:
    using Timeout::Timeout;
};

// ---------------------------------------------------------------------------
// Proxies / VerifySsl
// ---------------------------------------------------------------------------
class Proxies {
public:
    Proxies() = default;
    Proxies(const std::initializer_list<std::pair<std::string, std::string>>& l) {
        for (const auto& p : l) entries_[p.first] = p.second;
    }
    Proxies(const std::map<std::string, std::string>& m) : entries_(m) {}
    bool empty() const { return entries_.empty(); }
    const std::map<std::string, std::string>& map() const { return entries_; }
private:
    std::map<std::string, std::string> entries_;
};

class VerifySsl {
public:
    VerifySsl() = default;
    VerifySsl(bool v) : verify(v) {}
    bool verify{false};
};

// ---------------------------------------------------------------------------
// Parameter (single key/value) / Parameters (query string) / Payload (POST body)
// ---------------------------------------------------------------------------
class Parameter {
public:
    Parameter() = default;
    Parameter(std::string key, std::string value) : key_(std::move(key)), value_(std::move(value)) {}
    const std::string& key() const { return key_; }
    const std::string& value() const { return value_; }
private:
    std::string key_;
    std::string value_;
};

class Parameters {
public:
    Parameters() = default;
    Parameters(const std::initializer_list<Parameter>& l) : params_(l) {}
    void Add(const Parameter& p) { params_.push_back(p); }
    void Add(const std::initializer_list<Parameter>& l) {
        for (const auto& p : l) params_.push_back(p);
    }
    std::string GetContent(const CurlHolder&) const {
        std::string s;
        for (const auto& p : params_) {
            if (!s.empty()) s += '&';
            s += util::urlEncode(p.key());
            s += '=';
            s += util::urlEncode(p.value());
        }
        return s;
    }
    bool empty() const { return params_.empty(); }
private:
    std::vector<Parameter> params_;
};

class Payload {
public:
    Payload() = default;
    Payload(const std::initializer_list<Parameter>& l) : params_(l) {}
    void Add(const Parameter& p) { params_.push_back(p); }
    void Add(const std::initializer_list<Parameter>& l) {
        for (const auto& p : l) params_.push_back(p);
    }
    const std::vector<Parameter>& content() const { return params_; }
    bool empty() const { return params_.empty(); }
private:
    std::vector<Parameter> params_;
};

// Raw request body (analytics JSON, DLNA SOAP XML). Distinct from Payload
// (form-encoded key/value pairs): when SetBody is used the request is sent
// with the body verbatim and the caller's Content-Type header wins.
class Body {
public:
    Body() = default;
    Body(const char* s) : str_(s ? s : "") {}
    Body(const std::string& s) : str_(s) {}
    Body(std::string&& s) : str_(std::move(s)) {}
    const std::string& get() const { return str_; }
    bool empty() const { return str_.empty(); }
private:
    std::string str_;
};

// HTTP byte range (video_detail_api.cpp get_webmask). Translated to a
// "Range: bytes=start-end" request header by doRequest.
class Range {
public:
    Range() = default;
    Range(const std::optional<std::int64_t>& start, const std::optional<std::int64_t>& end)
        : start_(start), end_(end) {}
    bool hasStart() const { return start_.has_value(); }
    bool hasEnd() const { return end_.has_value(); }
    std::int64_t startVal() const { return *start_; }
    std::int64_t endVal() const { return *end_; }
    bool empty() const { return !start_.has_value() && !end_.has_value(); }
private:
    std::optional<std::int64_t> start_;
    std::optional<std::int64_t> end_;
};

// ---------------------------------------------------------------------------
// Cookie / Cookies
// ---------------------------------------------------------------------------
class Cookie {
public:
    Cookie() = default;
    Cookie(std::string name, std::string value) : name_(std::move(name)), value_(std::move(value)) {}
    Cookie(const std::pair<std::string, std::string>& p) : name_(p.first), value_(p.second) {}
    const std::string& GetName() const { return name_; }
    const std::string& GetValue() const { return value_; }
private:
    std::string name_;
    std::string value_;
};

class Cookies {
public:
    bool urlEncode{false};
    Cookies() = default;
    Cookies(bool p_urlEncode) : urlEncode(p_urlEncode) {}
    Cookies(const std::initializer_list<std::pair<std::string, std::string>>& pairs, bool p_urlEncode = false)
        : urlEncode(p_urlEncode) {
        for (const auto& p : pairs) cookies_.emplace_back(p.first, p.second);
    }
    void emplace_back(const Cookie& c) { cookies_.push_back(c); }
    using iterator = std::vector<Cookie>::iterator;
    using const_iterator = std::vector<Cookie>::const_iterator;
    iterator begin() { return cookies_.begin(); }
    iterator end() { return cookies_.end(); }
    const_iterator begin() const { return cookies_.begin(); }
    const_iterator end() const { return cookies_.end(); }
    bool empty() const { return cookies_.empty(); }
    std::size_t size() const { return cookies_.size(); }
    std::string GetEncoded(const CurlHolder&) const {
        std::string s;
        for (const auto& c : cookies_) {
            if (!s.empty()) s += "; ";
            s += c.GetName();
            s += '=';
            s += c.GetValue();
        }
        return s;
    }
private:
    std::vector<Cookie> cookies_;
};

// ---------------------------------------------------------------------------
// Error / Response
// ---------------------------------------------------------------------------
class Error {
public:
    Error() = default;
    Error(int c, std::string m) : code(c), message(std::move(m)) {}
    explicit operator bool() const { return code != 0; }
    int code{0};
    std::string message;
};

class Response {
public:
    Response() = default;
    int status_code{0};
    std::string text;
    Error error;
    Header header;
    Url url;
    std::string reason;
    std::size_t downloaded_bytes{0};
    Cookies cookies;
};

// ---------------------------------------------------------------------------
// ProgressCallback (image_helper uses a variadic lambda; signature doesn't
// matter because the shim does not honour progress / cancel mid-request).
// ---------------------------------------------------------------------------
class ProgressCallback {
public:
    using Fn = std::function<bool(double, double, double, double, std::int64_t, std::int64_t)>;
    ProgressCallback() = default;
    template <typename F>
    ProgressCallback(F&& f) : fn_(std::forward<F>(f)) {}
private:
    Fn fn_;
};

// ---------------------------------------------------------------------------
// Session — declared here, implemented in cpr_session.cpp.
// Internally copies its configuration into the async callback so the
// Session object itself may be destroyed immediately after GetCallback /
// PostCallback returns (matching wiliwili's usage in HTTP::_cpr_get/_cpr_post,
// where the shared_ptr<Session> goes out of scope).
// ---------------------------------------------------------------------------
class Session {
public:
    Session();
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void SetUrl(const Url& url);
    void SetParameters(const Parameters& p);
    void SetPayload(const Payload& p);
    void SetHeader(const Header& h);
    void SetCookies(const Cookies& c);
    void SetTimeout(const Timeout& t);
    void SetConnectTimeout(const ConnectTimeout& t);
    void SetProxies(const Proxies& p);
    void SetVerifySsl(const VerifySsl& v);
    void SetProgressCallback(const ProgressCallback& cb);
    void SetBody(const Body& b);
    void SetRange(const Range& r);

    // Returns a shared_ptr (not a reference) so callers can write
    // `session->GetCurlHolder()->handle` as they do with real cpr (real cpr
    // returns std::shared_ptr<CurlHolder> here). The handle is an inert
    // dummy on Android; curl_easy_setopt on it is a no-op (curl/curl.h).
    std::shared_ptr<CurlHolder> GetCurlHolder();

    Response Get();
    // Templated so callers may write GetCallback<>(...) (matches real cpr).
    // Implementation is inline because templates cannot live in the .cpp;
    // it captures config_ (shared_ptr) so the Session may be destroyed as
    // soon as GetCallback returns (wiliwili's HTTP::_cpr_get pattern).
    template <typename Cb>
    void GetCallback(Cb&& cb) {
        auto cfg = config_;
        std::thread([cfg, cb = std::forward<Cb>(cb)]() mutable {
            Response r = doRequest(*cfg, false);
            cb(r);
        }).detach();
    }
    template <typename Cb>
    void PostCallback(Cb&& cb) {
        auto cfg = config_;
        std::thread([cfg, cb = std::forward<Cb>(cb)]() mutable {
            Response r = doRequest(*cfg, true);
            cb(r);
        }).detach();
    }

private:
    struct Config;
    std::shared_ptr<Config> config_;
    static Response doRequest(const Config& cfg, bool isPost);
};

// ---------------------------------------------------------------------------
// MultiPerform — runs all added sessions concurrently and returns their
// responses in insertion order.
// ---------------------------------------------------------------------------
class MultiPerform {
public:
    MultiPerform() = default;
    void AddSession(const std::shared_ptr<Session>& s) { sessions_.push_back(s); }
    std::vector<Response> Get();
private:
    std::vector<std::shared_ptr<Session>> sessions_;
};

// ---------------------------------------------------------------------------
// Free-function convenience: cpr::GetCallback(cb, opts...) / PostCallback.
// Used by version_helper.cpp and dlna.cpp. Creates a Session, applies the
// option tuple via detail::applyOption (fold expression), then dispatches
// asynchronously. Callers ignore the return value.
// ---------------------------------------------------------------------------
namespace detail {
inline void applyOption(Session& s, const Url& o) { s.SetUrl(o); }
inline void applyOption(Session& s, const Header& o) { s.SetHeader(o); }
inline void applyOption(Session& s, const Body& o) { s.SetBody(o); }
inline void applyOption(Session& s, const Timeout& o) { s.SetTimeout(o); }
inline void applyOption(Session& s, const ConnectTimeout& o) { s.SetConnectTimeout(o); }
inline void applyOption(Session& s, const VerifySsl& o) { s.SetVerifySsl(o); }
inline void applyOption(Session& s, const Proxies& o) { s.SetProxies(o); }
inline void applyOption(Session& s, const Cookies& o) { s.SetCookies(o); }
inline void applyOption(Session& s, const Parameters& o) { s.SetParameters(o); }
inline void applyOption(Session& s, const Payload& o) { s.SetPayload(o); }
}  // namespace detail

template <typename Cb, typename... Opts>
void GetCallback(Cb&& cb, Opts&&... opts) {
    auto s = std::make_shared<Session>();
    (detail::applyOption(*s, std::forward<Opts>(opts)), ...);
    s->GetCallback(std::forward<Cb>(cb));
}

template <typename Cb, typename... Opts>
void PostCallback(Cb&& cb, Opts&&... opts) {
    auto s = std::make_shared<Session>();
    (detail::applyOption(*s, std::forward<Opts>(opts)), ...);
    s->PostCallback(std::forward<Cb>(cb));
}

// ---------------------------------------------------------------------------
// ThreadPool — minimal worker pool. cpr's ThreadPool is subclassed by
// wiliwili's ImageThreadPool (image_helper.cpp), so the public surface
// (constructor, Start/Stop/Submit, max_thread_num) must match.
// ---------------------------------------------------------------------------
class ThreadPool {
public:
    ThreadPool(std::size_t min_threads, std::size_t max_threads, std::chrono::milliseconds timeout);
    virtual ~ThreadPool();
    void Start();
    void Stop();
    void Submit(const std::function<void()>& fn);
    // Public because wiliwili's ImageThreadPool::setRequestThreads writes
    // both directly (image_helper.cpp:334). Real cpr also exposes these.
    std::size_t min_thread_num{0};
    std::size_t max_thread_num{0};
private:
    void worker();
    std::size_t min_threads_{0};
    std::chrono::milliseconds timeout_{0};
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_{false};
};

// ---------------------------------------------------------------------------
// cpr::async — wiliwili uses:
//   cpr::async::startup(min, max, timeout)   (config_helper.cpp)
//   cpr::async::cleanup()                    (config_helper.cpp)
//   cpr::async([]{ ... })                    (home_api.cpp, live_player_activity.cpp)
// In real cpr, `async` is BOTH a function template (returns AsyncWrapper)
// AND a class with static startup/cleanup methods — C++ allows a class and
// a function to share a name in the same namespace. The shim mirrors this:
// `async(fn)` launches a detached thread (call sites ignore the return
// value), and startup/cleanup are no-ops because OkHttp owns its own
// dispatcher / connection pool and does not need pre-warming.
// ---------------------------------------------------------------------------
template <class Fn>
auto async(Fn&& fn) {
    std::packaged_task<void()> task(std::forward<Fn>(fn));
    std::future<void> fut = task.get_future();
    std::thread(std::move(task)).detach();
    return fut;
}

class async {
public:
    static void startup(std::size_t /*min_threads*/, std::size_t /*max_threads*/,
                        std::chrono::milliseconds /*max_idle_ms*/) {}
    static void cleanup() {}
};

}  // namespace cpr
