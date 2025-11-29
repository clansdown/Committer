#include "default_prompt.hpp"

const std::string DEFAULT_LLM_INSTRUCTIONS = R"PROMPT(
Generate a commit message with a summary on the first line, then detailed, dense but concise description. 
If there are multiple, unrelated changes, prefer a list if they can be described in a single line.
I each unrelated change cannot be described in a single, preferred nested loops to describe them.
)PROMPT";