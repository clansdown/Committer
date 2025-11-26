# Commit

A utility to generate commit messages using LLMs for git repositories.

## Features

- Analyzes git diff
- Uses OpenRouter.ai or OpenCode.ai/Zen backends
- Generates commit messages with summary and description
- Configurable LLM instructions
- Optional file adding
- Builds to AppImage for portability

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
./scripts/package.sh
```

## Usage

```bash
./build/commit [--add] [--backend openrouter|zen] [--config config.txt]
```

Set API keys:
- OPENROUTER_API_KEY for openrouter
- ZEN_API_KEY for zen

Config file example:
```
instructions=Generate a commit message...
api_key=your_key
backend=openrouter
```
