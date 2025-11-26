# Agent Guidelines for Committer

## Build Commands
- **Build**: `./scripts/build.sh` (creates build/ directory and compiles with CMake)
- **Package**: `./scripts/package.sh` (creates distributable packages)
- **Clean**: `rm -rf build/`

## Test Commands
- No project-specific tests configured
- Dependencies include FTXUI tests (built automatically with CMake)

## Code Style Guidelines

### Language & Standards
- **C++ Standard**: C++23 required
- **Compiler**: GCC 15+ or compatible C++23 compiler
- **Headers**: Use `#pragma once` for include guards

### Imports & Includes
- **Order**: Standard library includes first, then third-party, then local includes
- **Local includes**: Use relative paths with quotes: `"git_utils.hpp"`
- **System includes**: Use angle brackets: `<iostream>`

### Naming Conventions
- **Functions**: snake_case (e.g., `get_repo_root()`, `add_files()`)
- **Classes**: PascalCase (e.g., `GitUtils`, `LLMBackend`)
- **Variables**: snake_case (e.g., `api_key`, `config_path`)
- **Constants**: UPPER_SNAKE_CASE (not extensively used)
- **Files**: snake_case with appropriate extensions (.cpp, .hpp)

### Formatting
- **Indentation**: 4 spaces (consistent throughout codebase)
- **Braces**: Opening brace on same line as function/class declaration
- **Line length**: No strict limit, but keep reasonable (<120 chars preferred)
- **Spacing**: Space after commas, around operators, before opening braces

### Error Handling
- **Exceptions**: Use `std::runtime_error` for recoverable errors
- **Return values**: Use appropriate return types (bool, string, vectors)
- **Error messages**: Descriptive and actionable

### Types & Memory
- **Smart pointers**: Use `std::unique_ptr` for ownership, `std::shared_ptr` sparingly
- **Containers**: Prefer `std::vector`, `std::string` over raw arrays
- **RAII**: Follow RAII principles, clean up resources in destructors

### Dependencies
- **libgit2**: For Git operations (use libgit2 functions, not system git commands)
- **libcurl**: For HTTP requests
- **CLI11**: For command-line argument parsing
- **nlohmann/json**: For JSON handling
- **FTXUI**: For terminal UI components

### Best Practices
- **Documentation**: No explicit documentation requirements, but code should be self-explanatory
- **Comments**: Minimal comments, prefer descriptive function/variable names
- **Modularity**: Keep functions focused on single responsibilities
- **Error checking**: Validate inputs and handle edge cases
- **Security**: Never log or expose API keys, validate all external inputs</content>
<parameter name="filePath">/home/ctl/projects/Committer/AGENTS.md