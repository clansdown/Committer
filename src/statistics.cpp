#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <map>
#include <optional>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include "statistics.hpp"
#include "git_utils.hpp"

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

TimingGuard::TimingGuard(bool enabled, const Config& config, const std::vector<GenerationResult>& generations, std::unique_ptr<LLMBackend>& llm, const std::string& repo_root, bool dry_run)
    : enabled_(enabled), config_(config), generations_(generations), llm_(llm), repo_root_(repo_root), dry_run_(dry_run), start_(std::chrono::high_resolution_clock::now()), llm_ms_(-1) {}

void TimingGuard::set_llm_time(long long ms) { llm_ms_ = ms; }

TimingGuard::~TimingGuard() {
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

                // Copy statistics directly from GenerationResult
                stats.input_tokens = gen.input_tokens;
                stats.output_tokens = gen.output_tokens;
                stats.total_cost = gen.total_cost;
                stats.latency = gen.latency;
                stats.generation_time = gen.generation_time;

            stats_list.push_back(stats);
        }

        // Always log basic info, enhanced when available
        std::string xdg_data_path = get_xdg_data_path() + "/generation_stats.log";
        log_generation_stats(stats_list, xdg_data_path);

        if (!repo_root_.empty()) {
            std::string repo_log_path = repo_root_ + "/.commit/generation_stats.log";
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