#include <CLI/CLI.hpp>
#include <iostream>
#include <memory>
#include <string>
#include "git_utils.hpp"
#include "config.hpp"
#include "llm_backend.hpp"

int main(int argc, char** argv) {
    CLI::App app{"commit - Generate commit messages using LLM"};

    bool add_files = false;
    bool no_add = false;
    std::string backend = "openrouter";
    std::string config_path = "config.txt";

    app.add_flag("--add", add_files, "Add files to staging before commit");
    app.add_flag("--no-add", no_add, "Do not add files, assume already staged");
    app.add_option("--backend", backend, "LLM backend: openrouter or zen", true);
    app.add_option("--config", config_path, "Path to config file", true);

    CLI11_PARSE(app, argc, argv);

    if (!GitUtils::is_git_repo()) {
        std::cerr << "Not a git repository\n";
        return 1;
    }

    Config config = Config::load_from_file(config_path);

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

    std::string commit_msg = llm->generate_commit_message(diff, config.llm_instructions);

    GitUtils::commit(commit_msg);

    std::cout << "Committed with message:\n" << commit_msg << std::endl;

    return 0;
}