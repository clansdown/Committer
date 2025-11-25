#include "config.hpp"
#include <fstream>
#include <iostream>

Config Config::load_from_file(const std::string& path) {
    Config config;
    config.llm_instructions = "Generate a commit message with a summary on the first line, then detailed description.";
    config.api_key = "";
    config.backend = "openrouter";
    config.model = "anthropic/claude-3-haiku";

    std::ifstream file(path);
    if (!file) {
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("instructions=") == 0) {
            config.llm_instructions = line.substr(13);
        } else if (line.find("api_key=") == 0) {
            config.api_key = line.substr(8);
        } else if (line.find("backend=") == 0) {
            config.backend = line.substr(8);
        } else if (line.find("model=") == 0) {
            config.model = line.substr(6);
        }
    }

    return config;
}