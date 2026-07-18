//
// cpr::Session / MultiPerform / ThreadPool shim implementation (Android).
//
// Session delegates the actual HTTP transport to bilibili::AndroidHttp
// (OkHttp over JNI). All cpr data-container types live in the header.
//
// GetCallback / PostCallback copy the session configuration into a detached
// worker thread so the Session object may be destroyed immediately after
// the call returns — matching wiliwili's HTTP::_cpr_get/_cpr_post pattern
// where the shared_ptr<Session> goes out of scope right after dispatching.
//

#include <cpr/cpr.h>  // the shim header (Android include path shadows real cpr)

#ifdef __ANDROID__

#include <algorithm>
#include <cctype>
#include <thread>

#include "platform/android_http.hpp"

namespace cpr {

// ---------------------------------------------------------------------------
// Session::Config
// ---------------------------------------------------------------------------
struct Session::Config {
    std::string url;
    Parameters parameters;
    Payload payload;
    Body body;
    Range range;
    Header header;
    Cookies cookies;
    std::int64_t timeoutMs{0};
    std::int64_t connectTimeoutMs{0};
    Proxies proxies;
    bool verifySsl{false};
    std::shared_ptr<CurlHolder> curlHolder{std::make_shared<CurlHolder>()};
};

Session::Session() : config_(std::make_shared<Config>()) {}
Session::~Session() = default;

void Session::SetUrl(const Url& url) { config_->url = url.str(); }
void Session::SetParameters(const Parameters& p) { config_->parameters = p; }
void Session::SetPayload(const Payload& p) { config_->payload = p; }
void Session::SetHeader(const Header& h) { config_->header = h; }
void Session::SetCookies(const Cookies& c) { config_->cookies = c; }
void Session::SetTimeout(const Timeout& t) { config_->timeoutMs = t.ms(); }
void Session::SetConnectTimeout(const ConnectTimeout& t) { config_->connectTimeoutMs = t.ms(); }
void Session::SetProxies(const Proxies& p) { config_->proxies = p; }
void Session::SetVerifySsl(const VerifySsl& v) { config_->verifySsl = v.verify; }
void Session::SetBody(const Body& b) { config_->body = b; }
void Session::SetRange(const Range& r) { config_->range = r; }
void Session::SetProgressCallback(const ProgressCallback&) {
    // No-op: OkHttp does not expose a per-tick progress callback that maps
    // to cpr's signature. Image cancellation is still honoured because
    // ImageHelper checks isCancel before and after the request.
}

std::shared_ptr<CurlHolder> Session::GetCurlHolder() { return config_->curlHolder; }

Response Session::doRequest(const Config& cfg, bool isPost) {
    Response r;
    r.url = Url(cfg.url);

    // Build full URL with query string (cpr sends Parameters on both GET
    // and POST; POST additionally carries Payload as the form body).
    std::string fullUrl = cfg.url;
    if (!cfg.parameters.empty()) {
        std::string q = cfg.parameters.GetContent(CurlHolder{});
        if (!q.empty()) {
            fullUrl += (fullUrl.find('?') == std::string::npos ? "?" : "&");
            fullUrl += q;
        }
    }

    // Cookies: cpr sessions carry cookies via SetCookies, but wiliwili mostly
    // stashes the global cookie jar in HTTP::HEADERS["cookie"]. Combine both
    // into a single Cookie request header and pass it out-of-band to OkHttp
    // (so duplicate Cookie headers can't occur).
    std::map<std::string, std::string> headers = cfg.header;
    std::string existingCookieKey;
    for (const auto& kv : headers) {
        std::string k = kv.first;
        std::transform(k.begin(), k.end(), k.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (k == "cookie") {
            existingCookieKey = kv.first;
            break;
        }
    }
    std::string cookieStr;
    if (!existingCookieKey.empty()) cookieStr = headers[existingCookieKey];
    std::string sessionCookie = cfg.cookies.GetEncoded(CurlHolder{});
    if (!sessionCookie.empty()) {
        if (!cookieStr.empty()) cookieStr += "; ";
        cookieStr += sessionCookie;
    }
    if (!existingCookieKey.empty()) headers.erase(existingCookieKey);

    // Range request → "Range: bytes=start-end" header (video_detail_api.cpp).
    if (!cfg.range.empty()) {
        std::string rangeVal = "bytes=";
        if (cfg.range.hasStart()) rangeVal += std::to_string(cfg.range.startVal());
        rangeVal += "-";
        if (cfg.range.hasEnd()) rangeVal += std::to_string(cfg.range.endVal());
        headers["Range"] = rangeVal;
    }

    bilibili::AndroidHttpResponse a;
    if (isPost) {
        if (!cfg.body.empty()) {
            // Raw body POST (analytics JSON, DLNA SOAP XML). Caller's
            // Content-Type header is forwarded as-is.
            a = bilibili::AndroidHttp::postRaw(fullUrl, cfg.body.get(), headers, cookieStr);
        } else {
            std::vector<std::pair<std::string, std::string>> formParams;
            formParams.reserve(cfg.payload.content().size());
            for (const auto& p : cfg.payload.content()) formParams.emplace_back(p.key(), p.value());
            a = bilibili::AndroidHttp::post(fullUrl, formParams, headers, cookieStr);
        }
    } else {
        a = bilibili::AndroidHttp::get(fullUrl, headers, cookieStr);
    }

    r.status_code = a.status_code;
    r.text = std::move(a.text);
    r.reason = std::move(a.reason);
    r.header = std::move(a.header);
    r.downloaded_bytes = r.text.size();
    if (a.hasError()) {
        r.error.code = -1;
        r.error.message = std::move(a.error);
    }
    for (const auto& sc : a.setCookies) {
        r.cookies.emplace_back(Cookie(sc.first, sc.second));
    }
    return r;
}

Response Session::Get() { return doRequest(*config_, false); }

// ---------------------------------------------------------------------------
// MultiPerform — run all sessions concurrently, preserve insertion order.
// ---------------------------------------------------------------------------
std::vector<Response> MultiPerform::Get() {
    std::vector<Response> results(sessions_.size());
    std::vector<std::thread> threads;
    threads.reserve(sessions_.size());
    for (std::size_t i = 0; i < sessions_.size(); ++i) {
        threads.emplace_back([i, this, &results]() {
            results[i] = sessions_[i]->Get();
        });
    }
    for (auto& t : threads) t.join();
    return results;
}

// ---------------------------------------------------------------------------
// ThreadPool — fixed pool of max_threads workers. Submits queue + cv.
// cpr::ThreadPool is subclassed by wiliwili::ImageThreadPool, hence the
// virtual destructor.
// ---------------------------------------------------------------------------
ThreadPool::ThreadPool(std::size_t min_threads, std::size_t max_threads,
                       std::chrono::milliseconds timeout)
    : min_thread_num(min_threads),
      max_thread_num(max_threads),
      min_threads_(min_threads),
      timeout_(timeout) {}

ThreadPool::~ThreadPool() { Stop(); }

void ThreadPool::Start() {
    stopping_ = false;
    std::size_t n = max_thread_num > 0 ? max_thread_num : 1;
    workers_.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        workers_.emplace_back([this] { worker(); });
    }
}

void ThreadPool::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return;
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

void ThreadPool::Submit(const std::function<void()>& fn) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(fn);
    }
    cv_.notify_one();
}

void ThreadPool::worker() {
    for (;;) {
        std::function<void()> fn;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) return;
            fn = std::move(queue_.front());
            queue_.pop();
        }
        if (fn) fn();
    }
}

}  // namespace cpr

#endif  // __ANDROID__
