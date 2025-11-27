# Commit

A utility to generate commit messages using LLMs for git repositories.

## Features

- Analyzes git diff
- Uses OpenRouter or Zen backends
- Generates commit messages with summary and description
- Configurable LLM instructions
- Optional file adding with interactive prompts for untracked files
- Dry-run mode to preview commits without executing
- List available models and query account balance
- Interactive configuration setup
- Support for custom LLM models
- Builds to AppImage and Snap for portability

## Dependencies

- CMake 3.20+
- libcurl
- libgit2
- C++23 compiler

## Build

```bash
./scripts/build.sh
```

## Package

```bash
./scripts/package.sh  # Creates AppImage
./scripts/package_snap.sh  # Creates Snap
```

## Usage

```bash
./build/commit [options]
```

Options:

- `-a,--add`: Add files to staging before commit
- `-n,--no-add`: Do not add files, assume already staged
- `--dry-run`: Generate commit message and print it without committing
- `--list-models`: List available models for the selected backend
- `-q,--query-balance`: Query available balance from the backend
- `--configure`: Configure the application interactively
- `-b,--backend`: LLM backend: openrouter or zen (default: openrouter)
- `--config`: Path to config file (default: ~/.config/commit/config.txt)
- `-m,--model`: LLM model to use

Set API keys via environment variables:
- `OPENROUTER_API_KEY` for openrouter
- `ZEN_API_KEY` for zen

## Configuration

Config file location: `~/.config/commit/config.txt`

Example config file:
```
backend=openrouter
model=gpt-4
instructions=Generate a commit message...
openrouter_api_key=your_key
zen_api_key=your_key
```

The tool will prompt for configuration if the config file doesn't exist.
