#pragma once

#include <string>

struct Config {
    std::string llm_instructions;
    std::string api_key;
    std::string backend;

    static Config load_from_file(const std::string& path);
};