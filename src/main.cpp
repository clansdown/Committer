#include <CLI/CLI.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <fstream>
#include "git_utils.hpp"
#include "config.hpp"
#include "llm_backend.hpp"

int main(int argc, char** argv) {
    CLI::App app{"commit - Generate commit messages using LLM"};

    bool add_files = false;
    bool no_add = false;
    bool list_models = false;
    bool query_balance = false;
    std::string backend = "openrouter";
    std::string config_path = "config.txt";
    std::string model = "";

    app.set_help_flag("--help", "Print help message");
    app.add_flag("--add", add_files, "Add files to staging before commit");
    app.add_flag("--no-add", no_add, "Do not add files, assume already staged");
    app.add_flag("--list-models", list_models, "List available models for the selected backend");
    app.add_flag("--query-balance", query_balance, "Query available balance from the backend");
    app.add_option("--backend", backend, "LLM backend: openrouter or zen");
    app.add_option("--config", config_path, "Path to config file");
    app.add_option("--model", model, "LLM model to use");

    CLI11_PARSE(app, argc, argv);

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
        std::cout << "Enter API key for " << backend << ": ";
        std::cin >> config_key;
        // save config
        config.backend = backend;
        std::ofstream file(config_path);
        file << "backend=" << config.backend << "\n";
        file << "model=" << config.model << "\n";
        file << "instructions=" << config.llm_instructions << "\n";
        if (!config.openrouter_api_key.empty()) file << "openrouter_api_key=" << config.openrouter_api_key << "\n";
        if (!config.zen_api_key.empty()) file << "zen_api_key=" << config.zen_api_key << "\n";
    };

    Config config = Config::load_from_file(config_path);
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

    if (add_files) {
        GitUtils::add_files();
    } else if (!no_add) {
        std::cout << "Add all files to staging? (y/n): ";
        char response;
        std::cin >> response;
        if (response == 'y' || response == 'Y') {
            GitUtils::add_files();
        }
    }

    std::string diff = GitUtils::get_diff();
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

    std::string commit_msg = llm->generate_commit_message(diff, config.llm_instructions, config.model);

    GitUtils::commit(commit_msg);

    std::cout << "Committed with message:\n" << commit_msg << std::endl;

    return 0;
}