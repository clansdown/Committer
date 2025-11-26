#include "config.hpp"
#include "git_utils.hpp"
#include "llm_backend.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
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

void configure_app(const std::string& config_path) {
    // Query terminal height
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    int terminal_height = ws.ws_row > 0 ? ws.ws_row : 24;

    Config existing = Config::load_from_file(config_path);

    enum class ConfigStep { Backend, ApiKey, Model, Instructions };
    ConfigStep current_step = ConfigStep::Backend;

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

    // Fetch models button
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
    auto fetch_button = ftxui::Button("Fetch Models", fetch_models);

    auto screen = ftxui::ScreenInteractive::TerminalOutput();
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
        file << "backend=" << full_existing.backend << "\n";
        file << "model=" << full_existing.model << "\n";
        file << "instructions=" << full_existing.llm_instructions << "\n";
        if (!full_existing.openrouter_api_key.empty()) file << "openrouter_api_key=" << full_existing.openrouter_api_key << "\n";
        if (!full_existing.zen_api_key.empty()) file << "zen_api_key=" << full_existing.zen_api_key << "\n";
        done = true;
        screen.ExitLoopClosure()();
    };

    ftxui::MenuOption backend_option;
    auto backend_menu = ftxui::Menu(&backends, &backend_index, backend_option);

    ftxui::InputOption api_key_option;
    auto api_key_input = ftxui::Input(&api_key, "API Key", api_key_option);

    ftxui::MenuOption model_option;
    auto model_menu = ftxui::Menu(&model_names, &model_index, model_option);

    ftxui::InputOption instructions_option;
    auto instructions_input = ftxui::Input(&instructions, "LLM Instructions", instructions_option);



    // Navigation buttons
    auto next_button = ftxui::Button("Next", [&]() {
        if (current_step == ConfigStep::Backend) {
            current_step = ConfigStep::ApiKey;
            api_key = trim((backends[backend_index] == "openrouter") ? existing.openrouter_api_key : existing.zen_api_key);
            api_key_input->TakeFocus();
        } else if (current_step == ConfigStep::ApiKey) {
            api_key = trim(api_key);
            if (!api_key.empty()) {
                current_step = ConfigStep::Model;
                fetch_models();
                model_menu->TakeFocus();
            }
        } else if (current_step == ConfigStep::Model) {
            current_step = ConfigStep::Instructions;
            instructions_input->TakeFocus();
        }
    });

    auto back_button = ftxui::Button("Back", [&]() {
        if (current_step == ConfigStep::ApiKey) {
            current_step = ConfigStep::Backend;
            backend_menu->TakeFocus();
        } else if (current_step == ConfigStep::Model) {
            current_step = ConfigStep::ApiKey;
            api_key_input->TakeFocus();
        } else if (current_step == ConfigStep::Instructions) {
            current_step = ConfigStep::Model;
            model_menu->TakeFocus();
        }
    });

    auto save_button = ftxui::Button("Save", perform_save);

    // Layout
    auto layout = ftxui::Container::Vertical(std::vector<ftxui::Component>{
        ftxui::Renderer(backend_menu, [&] { return current_step == ConfigStep::Backend ? ftxui::vbox(ftxui::text("Select Backend:"), backend_menu->Render()) : ftxui::text(""); }),
        ftxui::Renderer(api_key_input, [&] { return current_step == ConfigStep::ApiKey ? ftxui::vbox(ftxui::text("Enter API Key:"), api_key_input->Render()) : ftxui::text(""); }),
        ftxui::Renderer(fetch_button, [&] { return current_step == ConfigStep::Model ? ftxui::vbox(ftxui::text("Fetch available models:"), fetch_button->Render()) : ftxui::text(""); }),
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
        ftxui::Renderer(next_button, [&] { return current_step != ConfigStep::Instructions ? next_button->Render() : ftxui::text(""); }),
        ftxui::Renderer(back_button, [&] { return current_step != ConfigStep::Backend ? back_button->Render() : ftxui::text(""); }),
        ftxui::Renderer(save_button, [&] { return current_step == ConfigStep::Instructions ? save_button->Render() : ftxui::text(""); }),
    });

    auto renderer = ftxui::Renderer(layout, [&] {
        std::string step_title;
        if (current_step == ConfigStep::Backend) step_title = "Step 1/4: Select Backend";
        else if (current_step == ConfigStep::ApiKey) step_title = "Step 2/4: Enter API Key";
        else if (current_step == ConfigStep::Model) step_title = "Step 3/4: Select Model";
        else if (current_step == ConfigStep::Instructions) step_title = "Step 4/4: Edit Instructions";
        return ftxui::vbox(
            ftxui::text("Configuration Setup") | ftxui::bold,
            ftxui::text(step_title) | ftxui::dim,
            ftxui::separator(),
            layout->Render() | ftxui::flex,
            ftxui::separator(),
            ftxui::text("Use buttons to navigate, Enter to advance, Esc to cancel")
        ) | ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, terminal_height);
    });

    auto event_handler = ftxui::CatchEvent(renderer, [&](ftxui::Event event) {
        if (event == ftxui::Event::Return) {
            if (current_step == ConfigStep::Backend) {
                current_step = ConfigStep::ApiKey;
                api_key = trim((backends[backend_index] == "openrouter") ? existing.openrouter_api_key : existing.zen_api_key);
                api_key_input->TakeFocus();
            } else if (current_step == ConfigStep::ApiKey && !api_key.empty()) {
                api_key = trim(api_key);
                current_step = ConfigStep::Model;
                fetch_models();
                model_menu->TakeFocus();
            } else if (current_step == ConfigStep::Model) {
                current_step = ConfigStep::Instructions;
                instructions_input->TakeFocus();
            } else if (current_step == ConfigStep::Instructions) {
                perform_save();
            }
        } else if (event == ftxui::Event::Escape) {
            done = true;
            screen.ExitLoopClosure()();
        }
        return (event == ftxui::Event::Return || event == ftxui::Event::Escape);
    });

    backend_menu->TakeFocus();
    screen.Loop(event_handler);
}