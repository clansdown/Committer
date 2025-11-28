#pragma once

#include <string>
#include <vector>
#include <optional>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

inline size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

struct Model {
    std::string id;
    std::string name;
    std::string pricing;
    std::string description;
};

struct GenerationResult {
    std::string content;
    std::string generation_id;
    double input_tokens = -1;
    double output_tokens = -1;
    double total_cost = -1.0;
    double latency = -1.0;
    double generation_time = -1.0;
};

struct GenerationStats {
    std::string date;
    std::string backend;
    std::string model;
    std::string provider;
    double input_tokens = -1;
    double output_tokens = -1;
    double total_cost = -1.0;
    double latency = -1.0;
    double generation_time = -1.0;
    bool dry_run = false;
};

class LLMBackend {
public:
    virtual ~LLMBackend() = default;
    virtual void set_api_key(const std::string& key) = 0;
    virtual GenerationResult generate_commit_message(const std::string& diff, const std::string& instructions, const std::string& model, const std::string& provider = "", double temperature = -1.0) = 0;
    virtual std::vector<Model> get_available_models() = 0;
    virtual std::string get_balance() = 0;
};

class OpenRouterBackend : public LLMBackend {
public:
    void set_api_key(const std::string& key) override;
    GenerationResult generate_commit_message(const std::string& diff, const std::string& instructions, const std::string& model, const std::string& provider = "", double temperature = -1.0) override;
    std::vector<Model> get_available_models() override;
    std::string get_balance() override;

private:
    std::string api_key;
    GenerationResult handle_chat_response(const std::string& response, const std::string& payload);
    void fetch_generation_stats(GenerationResult& result, const std::string& generation_id);
};

class ZenBackend : public LLMBackend {
public:
    void set_api_key(const std::string& key) override;
    GenerationResult generate_commit_message(const std::string& diff, const std::string& instructions, const std::string& model, const std::string& provider = "", double temperature = -1.0) override;
    std::vector<Model> get_available_models() override;
    std::string get_balance() override;
private:
    std::string api_key;
    GenerationResult handle_chat_response(const std::string& response, const std::string& payload);
    std::vector<Model> parse_models_response(const std::string& response);
    void handle_api_error(const std::string& response, const std::string& error_msg);
    std::string get_pricing_for_model(const std::string& id);
    std::string get_endpoint_for_model(const std::string& model);
    nlohmann::json build_payload_for_model(const std::string& model, const std::string& instructions, const std::string& diff);
};