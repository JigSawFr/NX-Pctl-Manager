// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "util/http.hpp"

#include <curl/curl.h>
#include <mutex>

#include "app.hpp"

namespace http
{

static std::mutex s_lock;   // one request at a time; also guards the global init
static bool       s_ready = false;

namespace
{
struct Sink
{
    std::string* body;
    size_t       max;
    bool         overflow = false;
};

size_t on_data(char* data, size_t size, size_t count, void* user)
{
    auto* sink = static_cast<Sink*>(user);
    const size_t n = size * count;
    if (sink->body->size() + n > sink->max) {
        sink->overflow = true;
        return 0;   // aborts the transfer (CURLE_WRITE_ERROR)
    }
    sink->body->append(data, n);
    return n;
}
}   // namespace

namespace
{
// One request: GET when `post` is null, else a POST of `*post` as `content_type`.
bool perform(const std::string& url, const std::string* post, const char* content_type, std::string* body,
             std::string* error, size_t max_bytes, long timeout_s, long* status_out)
{
    std::lock_guard<std::mutex> guard(s_lock);
    body->clear();
    if (status_out) *status_out = 0;
    if (!s_ready) {
        // On the Switch this starts the ssl and csrng services (switch-curl's libnx backend).
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            if (error) *error = "libcurl initialisation failed";
            return false;
        }
        s_ready = true;
    }
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error) *error = "libcurl initialisation failed";
        return false;
    }

    Sink sink{ body, max_bytes };
    const std::string agent = "PlayGuard/" + app::version();
    char message[CURL_ERROR_SIZE] = "";
    struct curl_slist* headers = nullptr;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    // HTTPS only, redirects included (no downgrade to http:// or other schemes).
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, (long)CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, (long)CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout_s);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, agent.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, on_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, message);
    if (post) {
        // A redirect must not turn the POST into a GET elsewhere.
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post->data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)post->size());
        headers = curl_slist_append(headers, (std::string("Content-Type: ") + content_type).c_str());
        headers = curl_slist_append(headers, "Expect:");   // no 100-continue round trip
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    if (status_out) *status_out = status;

    if (sink.overflow) {
        if (error) *error = "response too large";
        return false;
    }
    if (rc != CURLE_OK) {
        if (error) *error = message[0] ? message : curl_easy_strerror(rc);
        return false;
    }
    if (status >= 400) {
        if (error) *error = "HTTP " + std::to_string(status);
        return false;
    }
    return true;
}
}   // namespace

bool get(const std::string& url, std::string* body, std::string* error, size_t max_bytes, long timeout_s)
{
    return perform(url, nullptr, nullptr, body, error, max_bytes, timeout_s, nullptr);
}

bool post(const std::string& url, const std::string& data, const char* content_type, std::string* body,
          std::string* error, long* status, size_t max_bytes, long timeout_s)
{
    return perform(url, &data, content_type, body, error, max_bytes, timeout_s, status);
}

void cleanup()
{
    // A request still running (the app quit during a check) would hold the
    // exit for up to its timeout: skip the cleanup, the process ends anyway.
    std::unique_lock<std::mutex> guard(s_lock, std::try_to_lock);
    if (!guard.owns_lock() || !s_ready) return;
    curl_global_cleanup();
    s_ready = false;
}

}   // namespace http
