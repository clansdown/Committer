#include "llm_backend.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include "llm_backend.hpp"

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

    nlohmann::json payload_json = {
        {"model", model},
        {"messages", {{
            {"role", "user"},
            {"content", instructions + "\n\nDiff:\n" + diff}
        }}}
    };
    std::string payload = payload_json.dump();

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
        std::cerr << "Failed to fetch URL: " << url << ", libcurl error: " << curl_easy_strerror(res) << std::endl;
        throw std::runtime_error("Curl error: " + std::string(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    return handle_chat_response(response, payload);
}

std::string OpenRouterBackend::handle_chat_response(const std::string& response, const std::string& payload) {
    try {
        nlohmann::json j = nlohmann::json::parse(response);
        if (j.contains("error")) {
            std::string error_msg = j["error"]["message"];
            try {
                std::ofstream query_file("/tmp/query.txt");
                query_file << payload;
                query_file.close();
                std::cerr << "Query saved to /tmp/query.txt" << std::endl;
            } catch (const std::exception& file_e) {
                std::cerr << "Warning: Failed to save query to /tmp/query.txt: " << file_e.what() << std::endl;
            }
            std::cerr << "API error: " << error_msg << std::endl;
            throw std::runtime_error("API error: " + error_msg);
        }
        return j["choices"][0]["message"]["content"];
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error in commit message generation: " << e.what() << std::endl;
        std::cerr << "Full response: " << response << std::endl;
        throw;
    }
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
    std::cout << "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nUsing API key: \"" << api_key << "\"" << std::endl;

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
        std::cerr << "Failed to fetch URL: " << url << ", libcurl error: " << curl_easy_strerror(res) << std::endl;
        throw std::runtime_error("Curl error: " + std::string(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    try {
        nlohmann::json j = nlohmann::json::parse(response);
        std::vector<Model> models;
        for (const auto& item : j["data"]) {
            Model m;
            m.id = item["id"];
            m.name = item["name"];
            m.description = item["description"];
        auto pricing = item["pricing"];
        double prompt = std::stod(pricing.value("prompt", "0.0"));
        double completion = std::stod(pricing.value("completion", "0.0"));
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << (prompt * 1000000) << "/1M input, $" << (completion * 1000000) << "/1M output";
        m.pricing = "$" + ss.str();
            models.push_back(m);
        }
        return models;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error in models query: " << e.what() << std::endl;
        std::cerr << "Full response: " << response << std::endl;
        throw;
    }
}

std::string OpenRouterBackend::get_balance() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init curl");
    }

    std::string url = "https://openrouter.ai/api/v1/credits";
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
        std::cerr << "Failed to fetch URL: " << url << ", libcurl error: " << curl_easy_strerror(res) << std::endl;
        throw std::runtime_error("Curl error: " + std::string(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    try {
        nlohmann::json j = nlohmann::json::parse(response);
        if (j.contains("data") && j["data"].contains("total_credits") && j["data"].contains("total_usage") &&
            !j["data"]["total_credits"].is_null() && !j["data"]["total_usage"].is_null()) {
            double total_credits = j["data"]["total_credits"];
            double total_usage = j["data"]["total_usage"];
            double balance = total_credits - total_usage;
            return "$" + std::to_string(balance);
        } else {
            std::cerr << "Balance query response: " << response << std::endl;
            throw std::runtime_error("Balance data not available or null in response");
        }
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error in balance query: " << e.what() << std::endl;
        std::cerr << "Full response: " << response << std::endl;
        throw;
    }
}