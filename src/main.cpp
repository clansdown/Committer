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
#include "git_utils.hpp"
#include "config.hpp"
#include "llm_backend.hpp"
#include "spinner.hpp"
#include "colors.hpp"

std::string get_xdg_data_path() {
    const char* xdg_data = std::getenv("XDG_DATA_HOME");
    std::string data_dir;
    if (xdg_data && strlen(xdg_data) > 0) {
        data_dir = xdg_data;
    } else {
        const char* home = std::getenv("HOME");
        if (!home || strlen(home) == 0) {
            throw std::runtime_error("HOME environment variable not set");
        }
        data_dir = std::string(home) + "/.local/share";
    }
    return data_dir + "/commit";
}

std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

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

void log_generation_stats(const std::vector<GenerationStats>& stats_list, const std::string& log_path) {
    std::filesystem::create_directories(std::filesystem::path(log_path).parent_path());
    std::ofstream file(log_path, std::ios::app);
    for (const auto& stats : stats_list) {
        nlohmann::json j = {
            {"date", stats.date},
            {"backend", stats.backend},
            {"model", stats.model},
            {"input_tokens", stats.input_tokens},
            {"output_tokens", stats.output_tokens},
            {"total_cost", stats.total_cost},
            {"latency", stats.latency},
            {"generation_time", stats.generation_time},
            {"dry_run", stats.dry_run}
        };
        if (!stats.provider.empty()) {
            j["provider"] = stats.provider;
        }
        file << j.dump() << std::endl;
    }
}

void summarize_generation_stats(const std::string& log_path) {
    if (!std::filesystem::exists(log_path)) {
        std::cout << "No generation stats found at " << log_path << std::endl;
        return;
    }

    std::ifstream file(log_path);
    std::string line;
    double total_cost = 0.0;
    double actual_cost = 0.0;
    double dry_run_cost = 0.0;
    int total_input_tokens = 0;
    int total_output_tokens = 0;
    int count = 0;
    int actual_count = 0;
    int dry_run_count = 0;
    std::map<std::string, int> model_counts;
    std::map<std::string, double> model_costs;

    while (std::getline(file, line)) {
        try {
            nlohmann::json j = nlohmann::json::parse(line);
            double cost = j.value("total_cost", -1.0);
            int input_tokens = j.value("input_tokens", -1);
            int output_tokens = j.value("output_tokens", -1);
            bool is_dry_run = j.value("dry_run", false);

            // Only count valid (non-negative) values
            if (cost >= 0) {
                total_cost += cost;
                if (is_dry_run) {
                    dry_run_cost += cost;
                } else {
                    actual_cost += cost;
                }
            }
            if (input_tokens >= 0) {
                total_input_tokens += input_tokens;
            }
            if (output_tokens >= 0) {
                total_output_tokens += output_tokens;
            }

            count++;
            if (is_dry_run) {
                dry_run_count++;
            } else {
                actual_count++;
            }

            std::string model = j.value("model", "unknown");
            model_counts[model]++;
            if (cost >= 0) {
                model_costs[model] += cost;
            }
        } catch (const nlohmann::json::exception&) {
            // Skip invalid lines
        }
    }

    if (count == 0) {
        std::cout << "No valid generation stats found" << std::endl;
        return;
    }

    std::cout << "Generation Statistics Summary:" << std::endl;
    std::cout << "Total generations: " << count;
    if (dry_run_count > 0) {
        std::cout << " (" << actual_count << " actual, " << dry_run_count << " dry runs)";
    }
    std::cout << std::endl;
    std::cout << "Total cost: $" << std::fixed << std::setprecision(4) << total_cost;
    if (dry_run_cost > 0) {
        std::cout << " ($" << std::fixed << std::setprecision(4) << actual_cost << " actual, $" << std::fixed << std::setprecision(4) << dry_run_cost << " dry runs)";
    }
    std::cout << std::endl;
    std::cout << "Total input tokens: " << total_input_tokens << std::endl;
    std::cout << "Total output tokens: " << total_output_tokens << std::endl;
    std::cout << "Average cost per generation: $" << std::fixed << std::setprecision(4) << (total_cost / count) << std::endl;
    if (actual_count > 0) {
        std::cout << "Average cost per actual generation: $" << std::fixed << std::setprecision(4) << (actual_cost / actual_count) << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Cost by model:" << std::endl;
    for (const auto& [model, cost] : model_costs) {
        std::cout << "  " << model << ": $" << std::fixed << std::setprecision(4) << cost << " (" << model_counts[model] << " generations)" << std::endl;
    }
}

class TimingGuard {
public:
    TimingGuard(bool enabled, const Config& config, const std::vector<GenerationResult>& generations, std::unique_ptr<LLMBackend>& llm, bool dry_run = false)
        : enabled_(enabled), config_(config), generations_(generations), llm_(llm), dry_run_(dry_run), start_(std::chrono::high_resolution_clock::now()), llm_ms_(-1) {}
    void set_llm_time(long long ms) { llm_ms_ = ms; }
    ~TimingGuard() {
        // Query and log generation stats (backend-agnostic) - ALWAYS runs
        if (!generations_.empty()) {
            std::vector<GenerationStats> stats_list;
            for (const auto& gen : generations_) {
                GenerationStats stats;
                stats.date = get_current_timestamp();
                stats.backend = config_.backend;
                stats.model = config_.model;
                stats.provider = config_.provider;
                stats.dry_run = dry_run_;
                stats.input_tokens = -1;   // Unknown until detailed stats loaded
                stats.output_tokens = -1;  // Unknown until detailed stats loaded
                stats.total_cost = -1.0;   // Unknown until detailed stats loaded

                // Try to enhance with detailed stats (OpenRouter only)
                if (!gen.generation_id.empty() && llm_ && dynamic_cast<OpenRouterBackend*>(llm_.get())) {
                    auto openrouter = dynamic_cast<OpenRouterBackend*>(llm_.get());
                    auto detailed_stats = openrouter->get_generation_stats(gen.generation_id, dry_run_, 3, config_.generation_stats_delay_ms);
                    if (detailed_stats) {
                        stats = *detailed_stats; // Override with real data
                    }
                }

                stats_list.push_back(stats);
            }

            // Always log basic info, enhanced when available
            std::string xdg_data_path = get_xdg_data_path() + "/generation_stats.log";
            log_generation_stats(stats_list, xdg_data_path);

            std::string repo_root = GitUtils::get_repo_root();
            if (!repo_root.empty()) {
                std::string repo_log_path = repo_root + "/.commit/generation_stats.log";
                log_generation_stats(stats_list, repo_log_path);
            }
        }

        if (enabled_) {
            std::cout << "\033[34mModel: " << config_.model;
            if (!config_.provider.empty()) {
                std::cout << " (provider: " << config_.provider << ")";
            }
            if (config_.temperature >= 0.0) {
                std::cout << " temperature: " << config_.temperature;
            }
            std::cout << "\033[0m" << std::endl;
            auto end = std::chrono::high_resolution_clock::now();
            auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
            auto format_time = [](long long ms) {
                if (ms < 1000) return std::to_string(ms) + "ms";
                double sec = ms / 1000.0;
                std::stringstream ss; ss << std::fixed << std::setprecision(2) << sec << "s";
                return ss.str();
            };
            std::cout << "\033[34m" << "Total time: " << "\033[37;44m" << format_time(total_ms) << "\033[34;49m";
            if (llm_ms_ >= 0) {
                std::cout << " LLM query time: " << "\033[37;44m" << format_time(llm_ms_) << "\033[34;49m";
            }
            std::cout << "\033[0m" << std::endl;
        }
    }
private:
    bool enabled_;
    const Config& config_;
    const std::vector<GenerationResult>& generations_;
    std::unique_ptr<LLMBackend>& llm_;
    bool dry_run_;
    std::chrono::high_resolution_clock::time_point start_;
    long long llm_ms_;
};

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
    bool time_run = false;
    bool summary = false;
    bool global_summary = false;
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
    app.add_option("-b,--backend", backend, "LLM backend: openrouter or zen");
    app.add_option("--config", config_path, "Path to config file");
    app.add_option("-m,--model", model, "LLM model to use");
    app.add_option("--provider", provider, "Model provider to use");
    app.add_option("--temperature", temperature, "Temperature for chat generation (0.0-2.0)");

    CLI11_PARSE(app, argc, argv);

    if (summary || global_summary) {
        if (summary) {
            std::string repo_root = GitUtils::get_repo_root();
            if (!repo_root.empty()) {
                std::string repo_log_path = repo_root + "/.commit/generation_stats.log";
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
        if (!config.openrouter_api_key.empty()) file << "openrouter_api_key=" << config.openrouter_api_key << "\n";
        if (!config.zen_api_key.empty()) file << "zen_api_key=" << config.zen_api_key << "\n";
    };

    Config config = Config::load_from_file(config_path);

    if (time_run) {
        config.time_run = true;
    }
    // else: keep config.time_run as loaded from file

    // Check and update .gitignore early
    std::string repo_root = GitUtils::get_repo_root();
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
    TimingGuard guard(config.time_run, config, generations, llm, dry_run);
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

        TimingGuard guard(config.time_run, config, generations, llm, false);

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