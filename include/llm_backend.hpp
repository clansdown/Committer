#pragma once

#include <string>
#include <vector>
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

class LLMBackend {
public:
    virtual ~LLMBackend() = default;
    virtual std::string generate_commit_message(const std::string& diff, const std::string& instructions, const std::string& model) = 0;
    virtual std::vector<Model> get_available_models() = 0;
    virtual std::string get_balance() = 0;
};

class OpenRouterBackend : public LLMBackend {
public:
    std::string generate_commit_message(const std::string& diff, const std::string& instructions, const std::string& model) override;
    std::vector<Model> get_available_models() override;
    std::string get_balance() override;
};

class ZenBackend : public LLMBackend {
public:
    std::string generate_commit_message(const std::string& diff, const std::string& instructions, const std::string& model) override;
    std::vector<Model> get_available_models() override;
    std::string get_balance() override;
};