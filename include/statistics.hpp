#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include "llm_backend.hpp"
#include "config.hpp"
#include "colors.hpp"

std::string get_xdg_data_path();
std::string get_current_timestamp();

void log_generation_stats(const std::vector<GenerationStats>& stats_list, const std::string& log_path);

void summarize_generation_stats(const std::string& log_path);

class TimingGuard {
public:
    TimingGuard(bool enabled, const Config& config, const std::vector<GenerationResult>& generations, std::unique_ptr<LLMBackend>& llm, const std::string& repo_root, bool dry_run = false);
    void set_llm_time(long long ms);
    ~TimingGuard();
private:
    bool enabled_;
    const Config& config_;
    const std::vector<GenerationResult>& generations_;
    std::unique_ptr<LLMBackend>& llm_;
    std::string repo_root_;
    bool dry_run_;
    std::chrono::high_resolution_clock::time_point start_;
    long long llm_ms_;
};