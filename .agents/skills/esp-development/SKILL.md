---
name: esp-development
description: ESP32 hardware and ESP-IDF firmware development conventions, build commands, version-aware virtual environment setup, target-specific strapping GPIO checks, dependency matrix tracking, and MCP server integrations.
---

# ESP32 & ESP-IDF Development Skill

## 1. MCP Server Discovery & Protocol

Before executing build commands or researching ESP-IDF APIs:

1. **ESP Documentation MCP Server (`espressif-docs`)**:
   - Check if the `espressif-docs` MCP server is available.
   - When available, ALWAYS use `search_espressif_sources` as the primary reference for documentation, SDK configuration options (`sdkconfig`), target-specific hardware details, and API signatures.

2. **ESP-IDF Build MCP Server**:
   - Check if an ESP-IDF Build / Tooling MCP server is active in the session context.
   - If available, prefer using its native MCP tools for building, flashing, erasing, and target configuration over manual shell invocation.

---

## 2. ESP-IDF Version & Virtual Environment Matrix

Workflows differ depending on the target ESP-IDF framework version line:

### Case A: Anterior to v5.5 (`< v5.5`)
- **Environment Setup**: Standard legacy exported environment. Ensure `. $IDF_PATH/export.sh` or standard Python virtual environment (`$IDF_TOOLS_PATH`) is sourced.
- **Build Commands**:
  - Set Target: `idf.py set-target <target>` (e.g. `esp32`, `esp32s3`, `esp32c3`)
  - Build: `idf.py build`
  - Flash: `idf.py -p PORT flash`
  - Monitor: `idf.py -p PORT monitor`

### Case B: Posterior to v5.5 (`>= v5.5` & `< v6.0`)
- **Environment Setup**:
  1. Check if an ESP-IDF Build MCP server is present. If so, invoke build/flash actions via MCP tools.
  2. If Build MCP server is absent, use `eim run "idf.py fullclean build"` from project root (`BypassSandbox: true`) or standard exported shell environment (`. $IDF_PATH/export.sh`).
- **Build Commands**: Standard `idf.py fullclean build` CLI workflow with modern ESP-IDF v5 component manager and CMake conventions.

### Case C: Above or Equal to v6.0 (`>= v6.0`)
- **Environment Setup**:
  1. Check if an ESP-IDF Build MCP server is available. If present, use MCP tools.
  2. **If Build MCP Server is NOT Available**: Use `eim` (Espressif Installation Manager) as the virtual environment manager for setting up and executing `idf.py`.
     - Execute commands via `eim` from project root in `BypassSandbox: true` mode: `eim run "idf.py fullclean build"`.
- **Build Commands**: `eim run "idf.py fullclean build"` (or native MCP tools when present).

---

## 3. Mandatory Build Rules & Subagent Protocol

### Kconfig & `sdkconfig` Mandatory `fullclean` Rule
- If any `Kconfig`, `Kconfig.projbuild`, `sdkconfig`, or `sdkconfig.*` defaults/override file is added, modified, or deleted, a `fullclean` build (`idf.py fullclean build` or `eim run "idf.py fullclean build"`) is **strictly mandatory** before building to ensure CMake cache re-evaluation.

### Multi-Agent Subagent Execution & Build Rules
- **Sequential Build Enforcement**: Subagents are strictly prohibited from executing build commands concurrently. Any control/verification builds must be run sequentially or yielded to the parent overseer agent.
- **Workspace & Branch Boundaries**: Subagents must stay within project root workspace boundaries and operate on isolated feature branches.

### Pre-Build Clean Protocol (`fullclean`)
Before executing initial builds at session start or immediately after switching Git branches:
- **Direct CLI / eim**: Run `idf.py fullclean` (or `eim run "idf.py fullclean" <env>`) before `idf.py build`.
- **ESP Build MCP Server**: Invoke the clean build tool action prior to running target builds to purge stale CMake cache and generated Kconfig headers.

---

## 3. Key Hardware, Firmware & Scope Checklists

### Pre-Build Session & Branch Switch Clean Checklist
- [ ] Run `idf.py fullclean` (or `eim run "idf.py fullclean" <env>`) at the start of a session or after a branch switch before attempting `idf.py build`.

### Agent Scope & Constraint Verification
- Check for existing `REQUIREMENTS.md` and `CONSTRAINTS.md` files in the repository root. Ensure code modifications conform to declared architecture limits and operational scope.

### Task Stack Allocation
- Verify FreeRTOS stack allocation sizes. Recommended minimum task stack: **2048 to 4096 bytes** for tasks performing C standard library formatting (`printf`/`sprintf`), file system operations, or networking.

### Target-Specific Strapping GPIO Verification
- **Do NOT apply a generic blanket list of strapping pins across all chip architectures.**
- Verify pin assignments against the **specific MCU build target**:
  - **ESP32 (Original)**: Strapping pins are GPIO 0, 2, 5, 12, 15.
  - **ESP32-S2**: Strapping pins are GPIO 0, 26, 45, 46.
  - **ESP32-S3**: Strapping pins are GPIO 0, 3, 45, 46.
  - **ESP32-C3**: Strapping pins are GPIO 2, 8, 9.
  - **ESP32-C6**: Strapping pins are GPIO 8, 9, 15.
  - **ESP32-H2**: Strapping pins are GPIO 8, 9, 25.
- Avoid using target-specific strapping pins for pull-up/pull-down critical hardware signals, buttons, or external SPI/I2C buses that could prevent normal booting or force bootloader mode.
- Use `search_espressif_sources` (via `espressif-docs` MCP server) to look up hardware datasheets and technical reference manuals for target-specific strapping pin configurations.

### External Dependency Matrix Audit
- For multi-subproject repositories using ESP-IDF component manager (`idf_component.yml`), verify that the top-level repository `README.md` maintains a structured dependency summary table listing each sub-project, invoked packages, exact installed versions from `dependencies.lock`/`managed_components`, and version constraints.
