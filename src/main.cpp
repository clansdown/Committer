#include <CLI/CLI.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <cstring>
#include "git_utils.hpp"
#include "config.hpp"
#include "llm_backend.hpp"
#include "spinner.hpp"
#include "colors.hpp"

std::string clean_commit_message(const std::string& msg) {
    std::string cleaned = msg;
    // Remove surrounding ```
    if (cleaned.size() >= 6 && cleaned.substr(0, 3) == "```" && cleaned.substr(cleaned.size() - 3) == "```") {
        cleaned = cleaned.substr(3, cleaned.size() - 6);
    }
    // Remove "diff" at start and end if present
    if (cleaned.size() >= 8 && cleaned.substr(0, 4) == "diff" && cleaned.substr(cleaned.size() - 4) == "diff") {
        cleaned = cleaned.substr(4, cleaned.size() - 8);
    }
    // Trim whitespace
    cleaned.erase(cleaned.begin(), std::find_if(cleaned.begin(), cleaned.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    cleaned.erase(std::find_if(cleaned.rbegin(), cleaned.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), cleaned.end());
    return cleaned;
}

std::string get_config_path() {
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::string config_dir;
    if (xdg_config && strlen(xdg_config) > 0) {
        config_dir = xdg_config;
    } else {
        const char* home = std::getenv("HOME");
        if (!home || strlen(home) == 0) {
            throw std::runtime_error("HOME environment variable not set");
        }
        config_dir = std::string(home) + "/.config";
    }
    return config_dir + "/commit/config.txt";
}

int main(int argc, char** argv) {
    CLI::App app{"commit - Generate commit messages using LLM"};

    bool add_files = false;
    bool no_add = false;
    bool list_models = false;
    bool query_balance = false;
    bool dry_run = false;
    bool configure = false;
    std::string backend = "openrouter";
    std::string config_path = get_config_path();
    std::string model = "";

    app.set_help_flag("--help", "Print help message");
    app.add_flag("-a,--add", add_files, "Add files to staging before commit");
    app.add_flag("-n,--no-add", no_add, "Do not add files, assume already staged");
    app.add_flag("--dry-run", dry_run, "Generate commit message and print it without committing");
    app.add_flag("--list-models", list_models, "List available models for the selected backend");
    app.add_flag("-q,--query-balance", query_balance, "Query available balance from the backend");
    app.add_flag("--configure", configure, "Configure the application interactively");
    app.add_option("-b,--backend", backend, "LLM backend: openrouter or zen");
    app.add_option("--config", config_path, "Path to config file");
    app.add_option("-m,--model", model, "LLM model to use");

    CLI11_PARSE(app, argc, argv);

    if (configure || !std::filesystem::exists(config_path)) {
        configure_app(config_path);
        if (configure) {
            return 0;
        }
    }

    try {
        auto get_api_key = [&](const std::string& backend, Config& config, const std::string& config_path) {
        std::string env_name = (backend == "openrouter") ? "OPENROUTER_API_KEY" : "ZEN_API_KEY";
        std::string& config_key = (backend == "openrouter") ? config.openrouter_api_key : config.zen_api_key;
        if (!config_key.empty()) {
            return;
        }
        char* env = getenv(env_name.c_str());
        if (env && strlen(env) > 0) {
            config_key = env;
            return;
        }
        std::cout << Colors::YELLOW << "Enter API key for " << backend << ": " << Colors::RESET;
        std::cin >> config_key;
        // save config
        config.backend = backend;
        std::filesystem::create_directories(std::filesystem::path(config_path).parent_path());
        std::ofstream file(config_path);
        file << "backend=" << config.backend << "\n";
        file << "model=" << config.model << "\n";
        file << "instructions=" << config.llm_instructions << "\n";
        if (!config.openrouter_api_key.empty()) file << "openrouter_api_key=" << config.openrouter_api_key << "\n";
        if (!config.zen_api_key.empty()) file << "zen_api_key=" << config.zen_api_key << "\n";
    };

    Config config = Config::load_from_file(config_path);

    // Auto-select backend if default and only one has a key
    if (backend == "openrouter" && config.openrouter_api_key.empty() && !config.zen_api_key.empty()) {
        backend = "zen";
    }

    get_api_key(backend, config, config_path);
    std::string api_key = (backend == "openrouter") ? config.openrouter_api_key : config.zen_api_key;

    if (!model.empty()) {
        config.model = model;
    } else {
        model = config.model;
    }

    if (list_models || query_balance) {
        std::unique_ptr<LLMBackend> llm;
        if (backend == "openrouter") {
            llm = std::make_unique<OpenRouterBackend>();
        } else if (backend == "zen") {
            llm = std::make_unique<ZenBackend>();
        } else {
            std::cerr << "Unknown backend\n";
            return 1;
        }
        llm->set_api_key(api_key);
        if (list_models) {
            auto models = llm->get_available_models();
            for (const auto& m : models) {
                std::cout << "ID: " << m.id << "\n";
                std::cout << "Name: " << m.name << "\n";
                std::cout << "Pricing: " << m.pricing << "\n";
                std::cout << "Description: " << m.description << "\n\n";
            }
        } else if (query_balance) {
            std::string balance = llm->get_balance();
            std::cout << "Available balance: " << balance << std::endl;
        }
        return 0;
    }

    auto tracked_modified = GitUtils::get_tracked_modified_files();
    auto unstaged_modified = GitUtils::get_unstaged_files();
    bool should_add_untracked = add_files;
    if (!no_add && !add_files) {
        auto untracked = GitUtils::get_untracked_files();
        if (!untracked.empty()) {
            std::cout << Colors::GREEN << "Untracked files:" << Colors::RESET << "\n";
            for (const auto& f : untracked) {
                std::cout << "  " << f << "\n";
            }
            std::cout << Colors::YELLOW << "Add all to staging? [Y/n]: " << Colors::RESET;
            std::string response;
            std::getline(std::cin, response);
            should_add_untracked = response.empty() || (response.size() > 0 && (response[0] == 'y' || response[0] == 'Y'));
        }
    }

    std::vector<std::string> files_to_add = tracked_modified;
    files_to_add.insert(files_to_add.end(), unstaged_modified.begin(), unstaged_modified.end());
    if (should_add_untracked) {
        auto untracked = GitUtils::get_untracked_files();
        files_to_add.insert(files_to_add.end(), untracked.begin(), untracked.end());
    }

    std::string diff = GitUtils::get_full_diff();
    if (diff.empty()) {
        std::cout << "No changes to commit\n";
        return 0;
    }

    std::unique_ptr<LLMBackend> llm;
    if (backend == "openrouter") {
        llm = std::make_unique<OpenRouterBackend>();
    } else if (backend == "zen") {
        llm = std::make_unique<ZenBackend>();
    } else {
        std::cerr << "Unknown backend\n";
        return 1;
    }
    llm->set_api_key(api_key);

    std::string commit_msg;
    {
        Spinner spinner("Generating commit message...");
        commit_msg = llm->generate_commit_message(diff, config.llm_instructions, config.model);
    }

    if (dry_run) {
        std::cout << std::endl;
        if (!files_to_add.empty()) {
            std::cout << Colors::GREEN << "[DRY RUN] Would add files:" << Colors::RESET << std::endl;
            for (const auto& f : files_to_add) {
                std::cout << "  " << f << "\n";
            }
        }
        std::cout << Colors::GREEN << "[DRY RUN] Would commit with message:" << Colors::RESET << std::endl;
        std::cout << commit_msg << std::endl;
    } else {
        try {
            GitUtils::add_files(files_to_add);
            auto [hash, output] = GitUtils::commit_with_output(commit_msg);
            std::cout << std::endl;
            if (!hash.empty()) {
                std::cout << Colors::BLUE << hash << Colors::RESET << " ";
            }
            std::cout << Colors::GREEN << "Committed with message:" << Colors::RESET << std::endl;
            std::cout << clean_commit_message(commit_msg) << std::endl;
        } catch (const std::runtime_error& e) {
            std::cerr << "Error during commit process: " << e.what() << std::endl;
            return 1;
        }
    }

    return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}