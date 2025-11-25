#pragma once

#include <string>
#include <curl/curl.h>

inline size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class LLMBackend {
public:
    virtual ~LLMBackend() = default;
    virtual std::string generate_commit_message(const std::string& diff, const std::string& instructions) = 0;
};

class OpenRouterBackend : public LLMBackend {
public:
    std::string generate_commit_message(const std::string& diff, const std::string& instructions) override;
};

class ZenBackend : public LLMBackend {
public:
    std::string generate_commit_message(const std::string& diff, const std::string& instructions) override;
};