#pragma once

#include <curl/curl.h>
#include <string>
#include <stdexcept>

class CurlRequest {
private:
    CURL* handle;
    curl_slist* headers;

public:
    CurlRequest() : handle(nullptr), headers(nullptr) {
        handle = curl_easy_init();
        if (!handle) {
            throw std::runtime_error("Failed to initialize CURL");
        }
    }

    ~CurlRequest() {
        if (handle) {
            curl_easy_cleanup(handle);
        }
        if (headers) {
            curl_slist_free_all(headers);
        }
    }

    // Delete copy constructor and assignment operator
    CurlRequest(const CurlRequest&) = delete;
    CurlRequest& operator=(const CurlRequest&) = delete;

    void set_url(const std::string& url) {
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    }

    void set_postfields(const std::string& data) {
        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data.c_str());
    }

    void set_get_method() {
        curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
    }

    void add_header(const std::string& header) {
        headers = curl_slist_append(headers, header.c_str());
    }

    void set_write_callback(size_t (*callback)(void*, size_t, size_t, void*), void* userdata) {
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, callback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, userdata);
    }

    CURLcode perform() {
        if (headers) {
            curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        }
        return curl_easy_perform(handle);
    }
};