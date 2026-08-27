---
name: rule-harvesting
description: Specialized workflow for discovering, extracting, generalizing, and cataloging new rules, constraints, and skills from specific or all workspace project repositories into antigravity-rules with user selection and approval.
---

# Workspace Rule & Skill Harvesting Skill

Use this skill when auditing workspace project repositories—either targeting a single specified project or scanning across all workspace repositories—to discover emerging practices, unique operational constraints, or localized `.geminirules` and propose them for centralized integration in `antigravity-rules`.

---

## Operating Modes

- **Targeted Project Mode**: Pass a specific target project directory (e.g. `/home/martinroger/Documents/superPod` or `CAN-logger-R56`). The workflow focuses specifically on discovering, extracting, and generalizing rules from that designated repository.
- **Full Workspace Audit Mode**: Scan all project repositories in the workspace root (e.g., `/home/martinroger/Documents/*`) to audit cross-project practices and gather rules systematically.

---

## Harvesting Workflow

### 1. Workspace Discovery & Scanning
Scan the specified target project (or all workspace projects in full audit mode) for rule definitions, documentation artifacts, and specialized configurations:
- **Local Rule Definitions**: Search for `.geminirules` or `.gemini/rules/*.geminirules` files inside the target directory.
- **Local Skill Packages**: Search for custom skills inside `.gemini/skills/*/SKILL.md` or `skills/*/SKILL.md`.
- **Constraint & Specification Documents**: Inspect `REQUIREMENTS.md`, `CONSTRAINTS.md`, `TOO.MD`, `SCENARIOS.md`, and top-level `README.md` files.
- **Build & Dependency Configs**: Inspect `CMakeLists.txt`, `sdkconfig`, `package.json`, `idf_component.yml`, and `dependencies.lock` for platform-specific edge cases or recurring fix patterns.

### 2. Analysis & Deduplication
Compare discovered rules and constraints against existing centralized rulesets in `antigravity-rules`:
- **[`rules/base.geminirules`](../../rules/base.geminirules)**: Core execution quality, requirement grilling, debugging, log inspection, max build retries, and industry protocol nomenclature.
- **[`rules/documentation.geminirules`](../../rules/documentation.geminirules)**: Markdown scope & constraint tracking, user-facing artifact previews, portable relative links, human-focused sub-project README / `TOO.MD` separation, Doxygen `@param` defaults, and `SCENARIOS.md` sequence diagrams.
- **[`rules/esp-idf.geminirules`](../../rules/esp-idf.geminirules)**: ESP32 hardware target, virtual environment (`eim`), `managed_components/` read-only boundary, FreeRTOS core pinning, strapping pin verification, dependency matrices, and HTTP server URI handler capacity checks.

### 3. Classification & Generalization
Evaluate each candidate practice and classify it into one of four tiers:
1. **Universal Base Rule**: Core execution, debugging, or code quality rule applicable across all projects. (Target: `rules/base.geminirules`).
2. **Documentation Rule**: Standards governing Markdown formatting, previews, links, or documentation structures. (Target: `rules/documentation.geminirules`).
3. **Platform/Domain-Specific Rule**: Rules specific to a hardware target, framework, or technology stack (e.g. ESP-IDF, Python, Web/React). (Target: `rules/<platform>.geminirules`).
4. **New Standalone Skill**: Recurring multi-step procedures or domain-specific workflows that warrant a dedicated `skills/<skill-name>/SKILL.md` package.

> [!TIP]
> **Generalization Principle**: Filter out hyper-project-specific naming (e.g. specific CAN message IDs or internal variable names) and rewrite the rule into a clean, reusable specification that applies broadly across similar projects or domains.

### 4. Proposal Generation & User Selection (No Autonomous Commit)
> [!IMPORTANT]
> **Human-in-the-Loop Constraint**: Do NOT autonomously commit or overwrite rules/skills in `antigravity-rules` without user selection and confirmation.
- **Render Proposal Artifact**: Per Section 2 of `rules/documentation.geminirules` (*Formatted Markdown Presentation*), generate a user-facing rendered Artifact (`user_facing: true`, `request_feedback: true`) detailing all newly harvested rules, skills, and catalog updates formatted directly as native Markdown elements.
- **User Selection & Approval**: Present the proposal for the user to review, select specific rules/skills to keep, modify, or discard.

### 5. Implementation, Cataloging & Global Sync (Post-Approval)
Upon explicit user approval and selection:
- Update target `.geminirules` files in `rules/`.
- Create new skill packages under `skills/<skill-name>/SKILL.md`.
- Update `README.md` catalog tables.
- Execute `./scripts/sync-global.sh` to refresh global symlinks in `~/.gemini/antigravity/skills/`.
- Summarize all exact file changes and request explicit user confirmation before creating a Git commit.
