#pragma once

#include <string>

struct Config {
    std::string llm_instructions;
    std::string backend;
    std::string model;
    std::string openrouter_api_key;
    std::string zen_api_key;
    bool time_run;
    std::string provider;
    double temperature;
    bool auto_push;

    static Config load_from_file(const std::string& path);
};

void configure_app(const std::string& config_path);