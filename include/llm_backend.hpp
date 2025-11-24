#pragma once

#include <string>

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