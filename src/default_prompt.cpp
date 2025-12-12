#include "default_prompt.hpp"

const std::string DEFAULT_LLM_INSTRUCTIONS = R"PROMPT(
Generate a git commit message with a summary on the first line (in grammatical English), then after it a detailed, 
dense but concise description of the changes. The description should be of the purpose of the changes, not a line-by-line 
accounting of what was changed.
In the detailed description, if there are multiple, unrelated changes, prefer a list to a paragraph.
If each unrelated change is composed of several sub-changes, prefer nested lists to describe them.
)PROMPT";