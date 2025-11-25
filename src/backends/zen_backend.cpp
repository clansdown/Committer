#include "llm_backend.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>

std::string ZenBackend::generate_commit_message(const std::string& diff, const std::string& instructions) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init curl");
    }

    std::string url = "https://opencode.ai/zen/api/v1/chat/completions"; // Assume this is the endpoint
    std::string api_key = std::getenv("ZEN_API_KEY");
    if (api_key.empty()) {
        throw std::runtime_error("ZEN_API_KEY not set");
    }

    std::string payload = R"(
    {
        "model": "zen-model",
        "messages": [
            {
                "role": "user",
                "content": ")";
    payload += instructions;
    payload += R"(\n\nDiff:\n)";
    payload += diff;
    payload += R"("
            }
        ]
    })";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        throw std::runtime_error("Curl error: " + std::string(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    // Parse response
    size_t pos = response.find("\"content\": \"");
    if (pos == std::string::npos) {
        throw std::runtime_error("Failed to parse response");
    }
    pos += 12;
    size_t end = response.find("\"", pos);
    return response.substr(pos, end - pos);
}