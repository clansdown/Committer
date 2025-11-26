#include "config.hpp"
#include "git_utils.hpp"
#include <fstream>
#include <iostream>
#include <map>

std::map<std::string, std::string> parse_config_file(const std::string& path) {
    std::map<std::string, std::string> values;
    std::ifstream file(path);
    if (!file) return values;
    std::string line;
    while (std::getline(file, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            values[key] = value;
        }
    }
    return values;
}

Config Config::load_from_file(const std::string& global_path) {
    Config config;
    config.llm_instructions = "Generate a commit message with a summary on the first line, then detailed description.";
    config.backend = "openrouter";
    config.model = "x-ai/grok-code-fast-1";

    // Load global config
    auto global_values = parse_config_file(global_path);
    if (global_values.count("instructions")) config.llm_instructions = global_values["instructions"];
    if (global_values.count("backend")) config.backend = global_values["backend"];
    if (global_values.count("model")) config.model = global_values["model"];
    if (global_values.count("openrouter_api_key")) config.openrouter_api_key = global_values["openrouter_api_key"];
    if (global_values.count("zen_api_key")) config.zen_api_key = global_values["zen_api_key"];

    // Load local config if exists
    std::string repo_root = GitUtils::get_repo_root();
    if (!repo_root.empty()) {
        std::string local_path = repo_root + "/.commit.conf";
        auto local_values = parse_config_file(local_path);
        // Override with local values
        if (local_values.count("instructions")) config.llm_instructions = local_values["instructions"];
        if (local_values.count("backend")) config.backend = local_values["backend"];
        if (local_values.count("model")) config.model = local_values["model"];
        if (local_values.count("openrouter_api_key")) config.openrouter_api_key = local_values["openrouter_api_key"];
        if (local_values.count("zen_api_key")) config.zen_api_key = local_values["zen_api_key"];
    }

    return config;
}