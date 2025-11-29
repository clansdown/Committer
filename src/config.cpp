#include "config.hpp"
#include "default_prompt.hpp"
#include "git_utils.hpp"
#include "llm_backend.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <filesystem>
#include <sstream>
#include <filesystem>
#include <sstream>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ftxui/screen/screen.hpp>

std::string trim(const std::string& s) {
    auto start = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    return (start < end) ? std::string(start, end) : std::string();
}

std::string read_file_content(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::map<std::string, std::string> parse_config_file(const std::string& path) {
    std::map<std::string, std::string> values;
    std::ifstream file(path);
    if (!file) return values;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
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
    config.llm_instructions = DEFAULT_LLM_INSTRUCTIONS;
    config.backend = "openrouter";
    config.model = "x-ai/grok-code-fast-1";
    config.time_run = false;
    config.provider = "";
    config.temperature = 0.25;

    // Load global config
    auto global_values = parse_config_file(global_path);
    if (global_values.count("backend")) config.backend = global_values["backend"];
    if (global_values.count("model")) config.model = global_values["model"];
    if (global_values.count("openrouter_api_key")) config.openrouter_api_key = global_values["openrouter_api_key"];
    if (global_values.count("zen_api_key")) config.zen_api_key = global_values["zen_api_key"];
    if (global_values.count("time_run")) config.time_run = (global_values["time_run"] == "true");
    if (global_values.count("provider")) config.provider = global_values["provider"];
    if (global_values.count("temperature")) config.temperature = std::stod(global_values["temperature"]);

    std::string global_prompt_path = std::filesystem::path(global_path).parent_path().string() + "/prompt.txt";
    if (std::filesystem::exists(global_prompt_path)) {
        std::string content = read_file_content(global_prompt_path);
        if (!content.empty()) {
            config.llm_instructions = content;
        }
    }

    // Load local config if exists
    std::string repo_root = GitUtils::get_repo_root();
    if (!repo_root.empty()) {
        std::string local_path = repo_root + "/.commit.conf";
        auto local_values = parse_config_file(local_path);
        // Override with local values
        if (local_values.count("backend")) config.backend = local_values["backend"];
        if (local_values.count("model")) config.model = local_values["model"];
        if (local_values.count("openrouter_api_key")) config.openrouter_api_key = local_values["openrouter_api_key"];
        if (local_values.count("zen_api_key")) config.zen_api_key = local_values["zen_api_key"];
        if (local_values.count("time_run")) config.time_run = (local_values["time_run"] == "true");
        if (local_values.count("provider")) config.provider = local_values["provider"];
        if (local_values.count("temperature")) config.temperature = std::stod(local_values["temperature"]);

        std::string local_prompt_path = repo_root + "/.commit/prompt.txt";
        if (std::filesystem::exists(local_prompt_path)) {
            std::string content = read_file_content(local_prompt_path);
            if (!content.empty()) {
                config.llm_instructions = content;
            }
        }
    }

    return config;
}

void configure_app(const std::string& config_path) {
    // Query terminal height
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    int terminal_height = ws.ws_row > 0 ? ws.ws_row : 24;

    Config existing = Config::load_from_file(config_path);

    enum class ConfigStep { Backend, ApiKey, Model, Instructions };

    // API key input
    std::string api_key;

    // Backend selection
    int backend_index = (existing.backend == "zen") ? 1 : 0;
    std::vector<std::string> backends = {"openrouter", "zen"};

    // Model selection
    std::vector<std::string> model_names;
    std::vector<std::string> model_ids;
    int model_index = 0;

    // Instructions input
    std::string instructions = existing.llm_instructions;

    // Fetch models function
    auto fetch_models = [&]() {
        std::string backend = backends[backend_index];
        std::unique_ptr<LLMBackend> llm;
        if (backend == "openrouter") {
            llm = std::make_unique<OpenRouterBackend>();
        } else if (backend == "zen") {
            llm = std::make_unique<ZenBackend>();
        } else {
            return;
        }
        llm->set_api_key(api_key);
        auto models = llm->get_available_models();
        model_names.clear();
        model_ids.clear();
        for (const auto& m : models) {
            model_names.push_back(m.name + " (" + m.pricing + ")");
            model_ids.push_back(m.id);
        }
        if (!model_names.empty()) {
            model_index = 0;
            // Set to existing model if found
            auto it = std::find(model_ids.begin(), model_ids.end(), existing.model);
            if (it != model_ids.end()) {
                model_index = std::distance(model_ids.begin(), it);
            }
        }
    };

    bool done = false;

    auto perform_save = [&]() {
        Config full_existing = Config::load_from_file(config_path);
        full_existing.backend = backends[backend_index];
        if (full_existing.backend == "openrouter") {
            full_existing.openrouter_api_key = api_key;
        } else {
            full_existing.zen_api_key = api_key;
        }
        if (!model_ids.empty()) {
            full_existing.model = model_ids[model_index];
        }
        full_existing.llm_instructions = instructions;
        // Save config
        std::filesystem::create_directories(std::filesystem::path(config_path).parent_path());
        // Backup existing config if it exists
        if (std::filesystem::exists(config_path)) {
            std::filesystem::copy_file(config_path, config_path + ".bak", std::filesystem::copy_options::overwrite_existing);
        }
        std::ofstream file(config_path);
        file << "# Backend to use for LLM requests (valid values: openrouter, zen)\n";
        file << "backend=" << full_existing.backend << "\n";
        file << "# Model ID to use for the selected backend\n";
        file << "model=" << full_existing.model << "\n";
        file << "# Provider to use for the model (optional)\n";
        file << "provider=" << full_existing.provider << "\n";
        file << "# Temperature for chat generation (0.0-2.0, optional)\n";
        file << "# temperature=0.7\n";
        file << "# Delay in milliseconds before querying generation stats (default: 100)\n";

        file << "# Custom instructions for commit message generation\n";
        file << "instructions=" << full_existing.llm_instructions << "\n";
        std::string prompt_path = std::filesystem::path(config_path).parent_path().string() + "/prompt.txt";
        std::ofstream prompt_file(prompt_path);
        if (prompt_file) {
            prompt_file << full_existing.llm_instructions;
        }
        if (!full_existing.openrouter_api_key.empty()) {
            file << "# API key for OpenRouter backend\n";
            file << "openrouter_api_key=" << full_existing.openrouter_api_key << "\n";
        }
        if (!full_existing.zen_api_key.empty()) {
            file << "# API key for Zen backend\n";
            file << "zen_api_key=" << full_existing.zen_api_key << "\n";
        }
        done = true;
    };

    ftxui::MenuOption backend_option;
    auto backend_menu = ftxui::Menu(&backends, &backend_index, backend_option);

    ftxui::MenuOption model_option;
    auto model_menu = ftxui::Menu(&model_names, &model_index, model_option);

    ftxui::InputOption instructions_option;
    auto instructions_input = ftxui::Input(&instructions, "LLM Instructions", instructions_option);

    // First FTXUI screen: Backend selection
    auto screen1 = ftxui::ScreenInteractive::TerminalOutput();
    bool backend_selected = false;

    auto layout1 = ftxui::Renderer(backend_menu, [&] { return ftxui::vbox(ftxui::text("Select Backend:"), backend_menu->Render()); });

    auto renderer1 = ftxui::Renderer(layout1, [&] {
        return ftxui::vbox(
            ftxui::text("Configuration Setup") | ftxui::bold,
            ftxui::text("Step 1/4: Select Backend") | ftxui::dim,
            ftxui::separator(),
            layout1->Render() | ftxui::flex,
            ftxui::separator(),
            ftxui::text("Press Enter to select, Esc to cancel")
        ) | ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, terminal_height);
    });

    auto event_handler1 = ftxui::CatchEvent(renderer1, [&](ftxui::Event event) {
        if (event == ftxui::Event::Return) {
            backend_selected = true;
            screen1.ExitLoopClosure()();
        } else if (event == ftxui::Event::Escape) {
            done = true;
            screen1.ExitLoopClosure()();
        }
        return false;
    });

    backend_menu->TakeFocus();
    screen1.Loop(event_handler1);

    if (!backend_selected || done) return;

    // API key input with colorized prompt
    std::cout << "\033[1;32mEnter API Key: \033[0m";
    std::getline(std::cin, api_key);
    api_key = trim(api_key);

    // Second FTXUI screen: Model and Instructions
    ConfigStep current_step = ConfigStep::Model;
    fetch_models();

    auto screen2 = ftxui::ScreenInteractive::TerminalOutput();

    // Layout for second screen
    auto layout2 = ftxui::Container::Vertical(std::vector<ftxui::Component>{
        ftxui::Renderer(model_menu, [&] {
            if (current_step != ConfigStep::Model) return ftxui::text("");
            std::string selected_info = model_names.empty() ? "No models loaded" : "Selected: " + model_names[model_index];
            return ftxui::vbox(
                ftxui::text(selected_info) | ftxui::bold,
                ftxui::text("Select Model:"),
                ftxui::frame(model_menu->Render()) | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, terminal_height - 10)
            );
        }),
        ftxui::Renderer(instructions_input, [&] { return current_step == ConfigStep::Instructions ? ftxui::vbox(ftxui::text("LLM Instructions:"), instructions_input->Render()) : ftxui::text(""); }),
    });

    auto renderer2 = ftxui::Renderer(layout2, [&] {
        std::string step_title;
        if (current_step == ConfigStep::Model) step_title = "Step 3/4: Select Model";
        else if (current_step == ConfigStep::Instructions) step_title = "Step 4/4: Edit Instructions";
        return ftxui::vbox(
            ftxui::text("Configuration Setup") | ftxui::bold,
            ftxui::text(step_title) | ftxui::dim,
            ftxui::separator(),
            layout2->Render() | ftxui::flex,
            ftxui::separator(),
            ftxui::text("Press Enter to advance, Esc to cancel")
        ) | ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, terminal_height);
    });

    auto event_handler2 = ftxui::CatchEvent(renderer2, [&](ftxui::Event event) {
        if (event == ftxui::Event::Return) {
            if (current_step == ConfigStep::Model) {
                current_step = ConfigStep::Instructions;
                instructions_input->TakeFocus();
            } else if (current_step == ConfigStep::Instructions) {
                perform_save();
                screen2.ExitLoopClosure()();
            }
        } else if (event == ftxui::Event::Escape) {
            done = true;
            screen2.ExitLoopClosure()();
        } else if (event.is_mouse()) {
            return true;
        }
        return (event == ftxui::Event::Return || event == ftxui::Event::Escape);
    });

    model_menu->TakeFocus();
    screen2.Loop(event_handler2);

    if (done) {
        std::cout << "Configuration saved to " << config_path << std::endl;
    }
}