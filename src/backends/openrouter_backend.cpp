#include "llm_backend.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include "llm_backend.hpp"

void OpenRouterBackend::set_api_key(const std::string& key) {
    api_key = key;
}

GenerationResult OpenRouterBackend::generate_commit_message(const std::string& diff, const std::string& instructions, const std::string& model, const std::string& provider, double temperature) {
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
    if (!provider.empty()) {
        payload_json["provider"] = {
            {"order", {provider}},
            {"allow_fallbacks", false}
        };
    }
    if (temperature >= 0.0) {
        payload_json["temperature"] = temperature;
    }
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

    GenerationResult result = handle_chat_response(response, payload);

    // Fetch detailed generation statistics
    if (!result.generation_id.empty()) {
        fetch_generation_stats(result, result.generation_id);
    }

    return result;
}

GenerationResult OpenRouterBackend::handle_chat_response(const std::string& response, const std::string& payload) {
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
        GenerationResult result;
        result.content = j["choices"][0]["message"]["content"];
        result.generation_id = j["id"];

        // Extract usage statistics if available
        if (j.contains("usage")) {
            auto& usage = j["usage"];
            result.input_tokens = usage.value("prompt_tokens", -1.0);
            result.output_tokens = usage.value("completion_tokens", -1.0);
            // Note: total_cost, latency, generation_time would need to be calculated or left as -1
            // since they're not directly in the usage response
        }

        return result;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error in commit message generation: " << e.what() << std::endl;
        std::cerr << "Full response: " << response << std::endl;
        throw;
    }
}

void OpenRouterBackend::fetch_generation_stats(GenerationResult& result, const std::string& generation_id) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        CURL* curl = curl_easy_init();
        if (!curl) {
            continue;
        }

        std::string url = "https://openrouter.ai/api/v1/generation?id=" + generation_id;
        if (api_key.empty()) {
            curl_easy_cleanup(curl);
            continue;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        if (res != CURLE_OK) {
            continue;
        }

        try {
            nlohmann::json j = nlohmann::json::parse(response);
            if (j.contains("data")) {
                auto& data = j["data"];
                result.total_cost = data.value("total_cost", -1.0);
                result.latency = data.value("latency", -1.0);
                result.generation_time = data.value("generation_time", -1.0);
                // Update token counts if more accurate data available
                if (data.contains("tokens_prompt") && data["tokens_prompt"].is_number()) {
                    result.input_tokens = data["tokens_prompt"];
                }
                if (data.contains("tokens_completion") && data["tokens_completion"].is_number()) {
                    result.output_tokens = data["tokens_completion"];
                }
                return; // Success
            }
        } catch (const nlohmann::json::exception&) {
            // Continue to next attempt
        }
    }
    // If all retries failed, leave stats as -1 (already set)
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

