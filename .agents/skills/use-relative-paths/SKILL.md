---
name: use-relative-paths
description: Ensure that all documentation, plans, and conversational responses use relative paths instead of absolute system paths.
---

# Use Relative Paths

When writing any documents (like markdown files, plans, READMEs) or generating text, you must **always** use relative paths to refer to files and directories within the repository.

## Instructions
1. **Never** write absolute paths (e.g., `c:\HYENG\dev_202607_hmi\src\main.cpp` or `/home/user/project/src/main.cpp`) in project documentation.
2. **Always** use relative paths from the project root (e.g., `src/main.cpp`, `_plan/plan.md`, `.agents/skills/`).
3. This applies to all generated code comments, markdown artifacts, development plans, and general textual descriptions.
4. (Note: When using internal tools like `view_file` or `write_to_file` which strictly require absolute paths as arguments, you must still provide the absolute path to the tool. This skill only applies to the text and documentation you write for the user or save in the repository.)
