#include "http_client.h"

#include <curl/curl.h>

#include <chrono>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

constexpr std::size_t max_html_size = 20 * 1024 * 1024;

struct CurlGlobal {
    CurlGlobal() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            throw std::runtime_error("Could not initialize the HTTP library.");
        }
    }
    ~CurlGlobal() { curl_global_cleanup(); }
};

using CurlHandle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;

std::string encode_title(const std::string& title) {
    CurlHandle curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) {
        throw std::runtime_error("Could not initialize the HTTP request.");
    }
    char* escaped = curl_easy_escape(curl.get(), title.c_str(), static_cast<int>(title.size()));
    if (escaped == nullptr) {
        throw std::runtime_error("Could not encode the page title.");
    }
    std::string encoded(escaped);
    curl_free(escaped);
    return encoded;
}

std::size_t receive_html(char* data, std::size_t size, std::size_t count, void* context) {
    std::string* html = static_cast<std::string*>(context);
    if (size != 0 && count > max_html_size / size) {
        return 0;
    }
    const std::size_t bytes = size * count;
    if (bytes > max_html_size - html->size()) {
        return 0;
    }
    html->append(data, bytes);
    return bytes;
}

bool is_transient(CURLcode result) {
    return result == CURLE_COULDNT_CONNECT || result == CURLE_COULDNT_RESOLVE_HOST ||
           result == CURLE_OPERATION_TIMEDOUT || result == CURLE_RECV_ERROR ||
           result == CURLE_SEND_ERROR || result == CURLE_GOT_NOTHING;
}

void validate_contact(const std::string& contact) {
    if (contact.empty()) {
        throw std::runtime_error("A contact email or website is required for the User-Agent.");
    }
    for (unsigned char c : contact) {
        if (std::iscntrl(c) || c == '(' || c == ')') {
            throw std::runtime_error("The contact value contains an invalid character.");
        }
    }
}

} // namespace

std::string article_url(const std::string& page_title) {
    static const CurlGlobal global;
    (void)global;
    return "https://en.wikipedia.org/wiki/" + encode_title(page_title);
}

std::string fetch_article_html(const std::string& page_title, const std::string& contact) {
    static const CurlGlobal global;
    (void)global;
    validate_contact(contact);
    const std::string endpoint = "https://en.wikipedia.org/w/rest.php/v1/page/" +
                                 encode_title(page_title) + "/html";
    const std::string user_agent = "CSE381WebScraper/0.1 (" + contact + ")";

    for (int attempt = 1; attempt <= 3; ++attempt) {
        CurlHandle curl(curl_easy_init(), curl_easy_cleanup);
        if (!curl) {
            throw std::runtime_error("Could not initialize the HTTP request.");
        }
        std::string html;
        char error[CURL_ERROR_SIZE] = {};
        curl_easy_setopt(curl.get(), CURLOPT_URL, endpoint.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, user_agent.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, receive_html);
        curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &html);
        curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error);
        curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
        curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");

        const CURLcode result = curl_easy_perform(curl.get());
        if (result != CURLE_OK) {
            if (result == CURLE_WRITE_ERROR && html.size() >= max_html_size) {
                throw std::runtime_error("The article is larger than the 20 MiB download limit.");
            }
            if (attempt < 3 && is_transient(result)) {
                std::this_thread::sleep_for(std::chrono::seconds(attempt));
                continue;
            }
            throw std::runtime_error("Could not fetch the article: " +
                                     std::string(error[0] ? error : curl_easy_strerror(result)));
        }

        long status = 0;
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
        if (status == 200) {
            return html;
        }
        if (status == 404) {
            throw std::runtime_error("Wikipedia page not found: " + page_title);
        }
        if (status == 429) {
            throw std::runtime_error("Wikipedia asked us to slow down (HTTP 429). Try again later.");
        }
        if (status >= 500 && attempt < 3) {
            std::this_thread::sleep_for(std::chrono::seconds(attempt));
            continue;
        }
        throw std::runtime_error("Wikipedia returned HTTP " + std::to_string(status) + ".");
    }
    throw std::runtime_error("Could not fetch the article after three attempts.");
}
