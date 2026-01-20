#include <CLI/CLI.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>
#include <optional>
#include <map>
#include <algorithm>
#include "git_utils.hpp"
#include "config.hpp"
#include "llm_backend.hpp"
#include "spinner.hpp"
#include "colors.hpp"
#include "statistics.hpp"



void check_and_add_commit_to_gitignore(const std::string& repo_root) {
    if (repo_root.empty()) return; // Not in git repo

    std::string gitignore_path = repo_root + "/.gitignore";

    // Read existing .gitignore
    std::string gitignore_content;
    std::ifstream gitignore_file(gitignore_path);
    if (gitignore_file) {
        std::stringstream buffer;
        buffer << gitignore_file.rdbuf();
        gitignore_content = buffer.str();
    }

    // Check if .commit/ is already ignored
    if (gitignore_content.find(".commit/") != std::string::npos) {
        return; // Already ignored
    }

    // Prompt user
    std::cout << "The .commit/ directory contains local generation statistics.\n";
    std::cout << "Add .commit/ to .gitignore? [Y/n] ";
    std::string response;
    std::getline(std::cin, response);

    // Default to yes if empty or starts with Y/y
    bool add_to_gitignore = response.empty() ||
                           (response.size() >= 1 &&
                           (response[0] == 'Y' || response[0] == 'y'));

    if (add_to_gitignore) {
        std::ofstream gitignore_out(gitignore_path, std::ios::app);
        if (gitignore_out) {
            gitignore_out << "\n# Local generation statistics\n.commit/\n";
            std::cout << "Added .commit/ to .gitignore\n";
        } else {
            std::cerr << "Warning: Could not update .gitignore\n";
        }
    }
}







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

std::vector<std::string> get_config_files(const std::string& config_path) {
    std::string repo_root = GitUtils::get_repo_root();
    std::vector<std::string> config_files;

    // Check global config
    if (std::filesystem::exists(config_path)) {
        config_files.push_back(config_path);
    }

    // Check global prompt
    std::string global_prompt = std::filesystem::path(config_path).parent_path().string() + "/prompt.txt";
    if (std::filesystem::exists(global_prompt)) {
        config_files.push_back(global_prompt);
    }

    // Check local configs if in repo
    if (!repo_root.empty()) {
        std::string local_config = repo_root + "/.commit/config.txt";
        if (std::filesystem::exists(local_config)) {
            config_files.push_back(local_config);
        }

        std::string local_prompt = repo_root + "/.commit/prompt.txt";
        if (std::filesystem::exists(local_prompt)) {
            config_files.push_back(local_prompt);
        }
    }

    return config_files;
}

int main(int argc, char** argv) {
    CLI::App app{"commit - Generate commit messages using LLM"};

    bool add_files = false;
    bool no_add = false;
    bool list_models = false;
    bool query_balance = false;
    bool dry_run = false;
    bool configure = false;
    bool time_run = false;
    bool summary = false;
    bool global_summary = false;
    bool push_flag = false;
    bool list_configs = false;
    bool print_repo_root = false;
    std::string backend = "openrouter";
    std::string config_path = get_config_path();
    std::string model = "";
    std::string provider = "";
    double temperature = 0.35;

    app.set_help_flag("--help", "Print help message");
    app.footer("Configuration file location: " + config_path);
    app.add_flag("-a,--add", add_files, "Add files to staging before commit");
    app.add_flag("-n,--no-add", no_add, "Do not add files, assume already staged");
    app.add_flag("--dry-run", dry_run, "Generate commit message and print it without committing");
    app.add_flag("--list-models", list_models, "List available models for the selected backend");
    app.add_flag("-q,--query-balance", query_balance, "Query available balance from the backend");
    app.add_flag("--configure", configure, "Configure the application interactively");
    app.add_flag("--time-run", time_run, "Time program execution and LLM query");
    app.add_flag("--summary", summary, "Show summary of generation costs from the local git repository");
    app.add_flag("--global-summary", global_summary, "Show summary of generation costs from the global log");
    app.add_flag("--push", push_flag, "Automatically push commits upstream after successful commit");
    app.add_flag("--list-configs", list_configs, "List all config files being read");
    app.add_flag("--repo-root", print_repo_root, "Print the git repository root directory");
    app.add_option("-b,--backend", backend, "LLM backend: openrouter or zen");
    app.add_option("--config", config_path, "Path to config file");
    app.add_option("-m,--model", model, "LLM model to use");
    app.add_option("--provider", provider, "Model provider to use");
    app.add_option("--temperature", temperature, "Temperature for chat generation (0.0-2.0)");

    CLI11_PARSE(app, argc, argv);

    if (list_configs) {
        auto config_files = get_config_files(config_path);
        std::cout << "Config files being read:" << std::endl;
        for (const auto& file : config_files) {
            std::cout << "  " << file << std::endl;
        }
        return 0;
    }

    if (print_repo_root) {
        std::string repo_root = GitUtils::get_repo_root();
        if (repo_root.empty()) {
            std::cout << "Not in a git repository" << std::endl;
            return 1;
        } else {
            std::cout << repo_root << std::endl;
            return 0;
        }
    }

    GitRepository repo;
    GitUtils git_utils(repo);

    if (summary || global_summary) {
        if (summary) {
            std::string repo_root = repo.get_repo_root();
            if (!repo_root.empty()) {
                std::string repo_log_path = repo.get_commit_dir() + "generation_stats.log";
                summarize_generation_stats(repo_log_path);
            } else {
                std::cout << "Not in a git repository" << std::endl;
            }
        }
        if (global_summary) {
            std::string global_log_path = get_xdg_data_path() + "/generation_stats.log";
            summarize_generation_stats(global_log_path);
        }
        return 0;
    }

    if (configure || !std::filesystem::exists(config_path)) {
        configure_app(config_path);
        if (configure) {
            return 0;
        }
    }

    try {
        auto get_api_key = [&](const std::string& backend, Config& config, const std::string& config_path) {
        std::string& config_key = (backend == "openrouter") ? config.openrouter_api_key : config.zen_api_key;
        if (!config_key.empty()) {
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
        file << "auto_push=" << (config.auto_push ? "true" : "false") << "\n";
        if (!config.openrouter_api_key.empty()) file << "openrouter_api_key=" << config.openrouter_api_key << "\n";
        if (!config.zen_api_key.empty()) file << "zen_api_key=" << config.zen_api_key << "\n";
    };

    Config config = Config::load_from_file(config_path);

    if (time_run) {
        config.time_run = true;
    }
    // else: keep config.time_run as loaded from file

    // Check and update .gitignore early
    std::string repo_root = repo.get_repo_root();
    check_and_add_commit_to_gitignore(repo_root);

    char* env_openrouter = getenv("OPENROUTER_API_KEY");
    if (env_openrouter && strlen(env_openrouter) > 0) {
        config.openrouter_api_key = env_openrouter;
    }
    char* env_zen = getenv("ZEN_API_KEY");
    if (env_zen && strlen(env_zen) > 0) {
        config.zen_api_key = env_zen;
    }

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

    if (!provider.empty()) {
        config.provider = provider;
    }

    if (temperature != 0.35) {
        config.temperature = temperature;
    }

    if (push_flag) {
        config.auto_push = true;
    }

    auto start_total = std::chrono::high_resolution_clock::now();

    std::vector<GenerationResult> generations;

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
        TimingGuard guard(config.time_run, config, generations, llm, repo.get_repo_root(), dry_run);
        if (list_models) {
            auto start_llm = std::chrono::high_resolution_clock::now();
            auto models = llm->get_available_models();
            auto end_llm = std::chrono::high_resolution_clock::now();
            auto llm_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_llm - start_llm).count();
            guard.set_llm_time(llm_ms);
            for (const auto& m : models) {
                std::cout << "ID: " << m.id << "\n";
                std::cout << "Name: " << m.name << "\n";
                std::cout << "Pricing: " << m.pricing << "\n";
                std::cout << "Description: " << m.description << "\n\n";
            }
        } else if (query_balance) {
            auto start_llm = std::chrono::high_resolution_clock::now();
            std::string balance = llm->get_balance();
            auto end_llm = std::chrono::high_resolution_clock::now();
            auto llm_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_llm - start_llm).count();
            guard.set_llm_time(llm_ms);
            std::cout << "Available balance: " << balance << std::endl;
        }
        return 0;
    }

    auto tracked_modified = git_utils.get_tracked_modified_files();
    auto unstaged_modified = git_utils.get_unstaged_files();
    std::vector<std::string> untracked;
    bool should_add_untracked = add_files;
    if (!no_add && !add_files) {
        untracked = git_utils.get_untracked_files();
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
        files_to_add.insert(files_to_add.end(), untracked.begin(), untracked.end());
    }

    std::string diff = git_utils.get_full_diff();
    // Append diffs for untracked files to be added
    for (const auto& file : files_to_add) {
        if (std::find(untracked.begin(), untracked.end(), file) != untracked.end()) {
            std::ifstream file_stream(file);
            if (!file_stream) continue;
            std::stringstream content;
            content << file_stream.rdbuf();
            std::string file_content = content.str();
            size_t line_count = std::count(file_content.begin(), file_content.end(), '\n') + (file_content.empty() ? 0 : 1);
            diff += "diff --git a/" + file + " b/" + file + "\n";
            diff += "new file mode 100644\n";
            diff += "index 0000000..e69de29\n";
            diff += "--- /dev/null\n";
            diff += "+++ b/" + file + "\n";
            diff += "@@ -0,0 +1," + std::to_string(line_count) + " @@\n";
            std::istringstream iss(file_content);
            std::string line;
            while (std::getline(iss, line)) {
                diff += "+" + line + "\n";
            }
            if (!file_content.empty() && file_content.back() != '\n') {
                diff += "\n\\ No newline at end of file\n";
            }
        }
    }
    if (diff.empty() && files_to_add.empty()) {
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

    TimingGuard guard(config.time_run, config, generations, llm, repo.get_repo_root(), dry_run);

    GenerationResult generation_result;
    {
        Spinner spinner("Generating commit message...");
        auto start_llm = std::chrono::high_resolution_clock::now();
        generation_result = llm->generate_commit_message(diff, config.llm_instructions, config.model, config.provider, config.temperature);
        auto end_llm = std::chrono::high_resolution_clock::now();
        auto llm_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_llm - start_llm).count();
        guard.set_llm_time(llm_ms);
        generations.push_back(generation_result);
    }
    std::string commit_msg = generation_result.content;

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
            git_utils.add_files(files_to_add);
            auto [hash, output] = git_utils.commit_with_output(commit_msg);
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

    if (!dry_run && config.auto_push) {
        try {
            git_utils.push();
            std::cout << Colors::GREEN << "Changes pushed upstream successfully." << Colors::RESET << std::endl;
        } catch (const std::runtime_error& e) {
            std::string error_msg = e.what();
            std::cout << Colors::YELLOW << "Warning: Failed to push changes upstream: " << error_msg << Colors::RESET << std::endl;
            // Check for upstream change indicators
            if (error_msg.find("non-fast-forward") != std::string::npos ||
                error_msg.find("updates were rejected") != std::string::npos ||
                error_msg.find("fetch first") != std::string::npos) {
                std::cout << Colors::YELLOW << "Suggestion: Pull upstream changes with 'git pull' before pushing." << Colors::RESET << std::endl;
            }
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