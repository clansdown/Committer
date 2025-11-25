#include "llm_backend.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

void OpenRouterBackend::set_api_key(const std::string& key) {
    api_key = key;
}

std::string OpenRouterBackend::generate_commit_message(const std::string& diff, const std::string& instructions, const std::string& model) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init curl");
    }

    std::string url = "https://openrouter.ai/api/v1/chat/completions";
    if (api_key.empty()) {
        throw std::runtime_error("API key not set");
    }

    std::string payload = R"(
    {
        "model": ")" + model + R"(",
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

    // Parse response, assume JSON, extract message
    // For simplicity, find "content": "..." 
    size_t pos = response.find("\"content\": \"");
    if (pos == std::string::npos) {
        throw std::runtime_error("Failed to parse response");
    }
    pos += 12;
    size_t end = response.find("\"", pos);
    return response.substr(pos, end - pos);
}

std::vector<Model> OpenRouterBackend::get_available_models() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init curl");
    }

    std::string url = "https://openrouter.ai/api/v1/models";
    if (api_key.empty()) {
        throw std::runtime_error("API key not set");
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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

    nlohmann::json j = nlohmann::json::parse(response);
    std::vector<Model> models;
    for (const auto& item : j["data"]) {
        Model m;
        m.id = item["id"];
        m.name = item["name"];
        m.description = item["description"];
        auto pricing = item["pricing"];
        double prompt = pricing["prompt"];
        double completion = pricing["completion"];
        m.pricing = "$" + std::to_string(prompt * 1000) + "/1K input, $" + std::to_string(completion * 1000) + "/1K output";
        models.push_back(m);
    }
    return models;
}

std::string OpenRouterBackend::get_balance() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init curl");
    }

    std::string url = "https://openrouter.ai/api/v1/auth/key";
    if (api_key.empty()) {
        throw std::runtime_error("API key not set");
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
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

    nlohmann::json j = nlohmann::json::parse(response);
    double balance = j["data"]["balance"];
    return "$" + std::to_string(balance);
}