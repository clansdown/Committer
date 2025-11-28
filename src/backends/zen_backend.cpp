#include "llm_backend.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <fstream>

void ZenBackend::set_api_key(const std::string& key) {
    api_key = key;
}

GenerationResult ZenBackend::generate_commit_message(const std::string& diff, const std::string& instructions, const std::string& model, const std::string& provider, double temperature) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init curl");
    }

    std::string url = get_endpoint_for_model(model);
    if (api_key.empty()) {
        throw std::runtime_error("API key not set");
    }

    nlohmann::json payload_json = build_payload_for_model(model, instructions, diff);
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

GenerationResult ZenBackend::handle_chat_response(const std::string& response, const std::string& payload) {
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
        // Try OpenAI format first
        if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty()) {
            result.content = j["choices"][0]["message"]["content"];
            // Zen may not provide generation IDs like OpenRouter, so leave empty
            result.generation_id = "";
        }
        // Try Anthropic format
        else if (j.contains("content") && j["content"].is_array() && !j["content"].empty()) {
            result.content = j["content"][0]["text"];
            result.generation_id = "";
        }
        // Fallback
        else {
            throw std::runtime_error("Unexpected response format");
        }
        return result;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error in commit message generation: " << e.what() << std::endl;
        std::cerr << "Full response: " << response << std::endl;
        throw;
    }
}

std::vector<Model> ZenBackend::get_available_models() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init curl");
    }

    std::string url = "https://opencode.ai/zen/v1/models";
    if (api_key.empty()) {
        throw std::runtime_error("API key not set");
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
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

    return parse_models_response(response);
}

std::string ZenBackend::get_balance() {
    throw std::runtime_error("Balance query not supported for Zen backend");
}

std::vector<Model> ZenBackend::parse_models_response(const std::string& response) {
    std::vector<Model> models;
    try {
        nlohmann::json j = nlohmann::json::parse(response);
        if (j.contains("error")) {
            handle_api_error(response, j["error"]["message"]);
        }
        if (!j.contains("data") || !j["data"].is_array()) {
            handle_api_error(response, "Expected JSON object with 'data' array");
        }
        for (const auto& model_json : j["data"]) {
            if (!model_json.is_object()) continue;
            std::string id = model_json.at("id");
            std::string name = model_json.value("name", id);
            std::string pricing = model_json.value("pricing", get_pricing_for_model(id));
            std::string description = model_json.value("description", "Model for coding agents.");
            models.push_back({id, name, pricing, description});
        }
    } catch (const nlohmann::json::exception& e) {
        handle_api_error(response, "JSON parsing error: " + std::string(e.what()));
    }
    return models;
}

void ZenBackend::handle_api_error(const std::string& response, const std::string& error_msg) {
    std::cerr << "Zen API error: " << error_msg << std::endl;
    if (response.size() > 1024) {
        try {
            std::ofstream error_file("/tmp/zen_models_error.txt");
            error_file << response;
            error_file.close();
            std::cerr << "Full response saved to /tmp/zen_models_error.txt" << std::endl;
        } catch (const std::exception& file_e) {
            std::cerr << "Warning: Failed to save response to /tmp/zen_models_error.txt: " << file_e.what() << std::endl;
            std::cerr << "Response: " << response << std::endl;
        }
    } else {
        std::cerr << "Response: " << response << std::endl;
    }
    throw std::runtime_error("Zen API error: " + error_msg);
}

std::string ZenBackend::get_pricing_for_model(const std::string& id) {
    // Hardcoded fallback from docs
    if (id == "gpt-5.1" || id == "gpt-5.1-codex" || id == "gpt-5" || id == "gpt-5-codex") return "$1.07/1M input, $8.50/1M output";
    if (id == "gpt-5-nano") return "Free";
    if (id == "claude-sonnet-4-5") return "$3.00/1M input, $15.00/1M output (≤200K), $6.00/1M input, $22.50/1M output (>200K)";
    // Add more as needed...
    return "Pricing not available";
}

std::string ZenBackend::get_endpoint_for_model(const std::string& model) {
    if (model.find("claude-") == 0) {
        return "https://opencode.ai/zen/v1/messages";
    } else if (model.find("gemini-") == 0) {
        return "https://opencode.ai/zen/v1/models/" + model;
    } else {
        // Default to OpenAI compatible
        return "https://opencode.ai/zen/v1/chat/completions";
    }
}

nlohmann::json ZenBackend::build_payload_for_model(const std::string& model, const std::string& instructions, const std::string& diff) {
    if (model.find("claude-") == 0) {
        // Anthropic format
        return {
            {"model", model},
            {"messages", {{
                {"role", "user"},
                {"content", instructions + "\n\nDiff:\n" + diff}
            }}},
            {"max_tokens", 1000}
        };
    } else {
        // OpenAI format
        return {
            {"model", model},
            {"messages", {{
                {"role", "user"},
                {"content", instructions + "\n\nDiff:\n" + diff}
            }}}
        };
    }
}