#!/usr/bin/env python3
"""
Interactive ESP-IDF Project Dependency Manager (VX Binocle)
============================================================
Checks and updates project dependencies for both factory applications and main
applications interactively across the repository:
  - Queries the Espressif Component Registry API for the latest available versions.
  - Queries Git remote tags for git-based dependencies (e.g. twai_daemon, binocan).
  - Interactively offers version bumps and updates idf_component.yml manifests.
  - Reconfigures, cleans (build/ + sdkconfig), and builds modified applications with verbose output.
  - Synchronizes the Global External Dependency Matrix in README.md.

Usage:
    python tools/manage_dependencies.py [--check-only] [--non-interactive] [--build] [--target-app {app_name}]
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request
import urllib.error
from pathlib import Path

# Workspace Root
REPO_ROOT = Path(__file__).resolve().parent.parent

PROJECT_CONFIGS = [
    {
        "key": "itf_main",
        "display_name": "Interface Board (Main)",
        "project_dir": REPO_ROOT / "interface_board",
        "manifest": REPO_ROOT / "interface_board" / "main" / "idf_component.yml",
        "category": "main",
    },
    {
        "key": "ldb_main",
        "display_name": "Left Display Board (Main)",
        "project_dir": REPO_ROOT / "left_screen",
        "manifest": REPO_ROOT / "left_screen" / "main" / "idf_component.yml",
        "category": "main",
    },
    {
        "key": "rdb_main",
        "display_name": "Right Display Board (Main)",
        "project_dir": REPO_ROOT / "right_screen",
        "manifest": REPO_ROOT / "right_screen" / "main" / "idf_component.yml",
        "category": "main",
    },
    {
        "key": "emu_main",
        "display_name": "Vehicle Emulator Console",
        "project_dir": REPO_ROOT / "emulator-console",
        "manifest": REPO_ROOT / "emulator-console" / "main" / "idf_component.yml",
        "category": "main",
    },
    {
        "key": "itf_factory",
        "display_name": "ITF Factory App",
        "project_dir": REPO_ROOT / "factory apps" / "ITF factory app",
        "manifest": REPO_ROOT / "factory apps" / "ITF factory app" / "main" / "idf_component.yml",
        "category": "factory",
    },
    {
        "key": "ldb_factory",
        "display_name": "Left Display Factory App",
        "project_dir": REPO_ROOT / "factory apps" / "left display factory app",
        "manifest": REPO_ROOT / "factory apps" / "left display factory app" / "main" / "idf_component.yml",
        "category": "factory",
    },
    {
        "key": "rdb_factory",
        "display_name": "Right Display Factory App",
        "project_dir": REPO_ROOT / "factory apps" / "right display factory app",
        "manifest": REPO_ROOT / "factory apps" / "right display factory app" / "main" / "idf_component.yml",
        "category": "factory",
    },
]

REGISTRY_API_BASE = "https://components.espressif.com/api/components"


def detect_build_runner() -> list[str]:
    """Detect whether to use eim or idf.py."""
    if shutil.which("eim"):
        return ["eim", "run"]
    if shutil.which("idf.py"):
        return ["idf.py"]
    return ["idf.py"]


def query_registry_latest_version(component_name: str) -> str | None:
    """Fetch the latest non-yanked version from Espressif Component Registry API."""
    url = f"{REGISTRY_API_BASE}/{component_name}"
    try:
        req = urllib.request.Request(
            url,
            headers={"User-Agent": "vx-binocle-dep-manager/1.0"},
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            versions = data.get("versions", [])
            for ver_entry in versions:
                if not ver_entry.get("yanked_at"):
                    v = ver_entry.get("version", "")
                    # Skip pre-release / build metadata versions like ~1, -rc, -beta unless all are pre-release
                    if not re.search(r"[~a-zA-Z]", v):
                        return v
            # Fallback to first non-yanked if only pre-releases exist
            for ver_entry in versions:
                if not ver_entry.get("yanked_at"):
                    return ver_entry.get("version")
    except Exception as e:
        # Fallback or silent fail
        return None
    return None


def query_git_remote_latest_tag(git_url: str) -> str | None:
    """Fetch the highest semantic version tag from a git remote repository."""
    try:
        res = subprocess.run(
            ["git", "ls-remote", "--tags", git_url],
            capture_output=True,
            text=True,
            check=True,
            timeout=15,
        )
        tags = []
        for line in res.stdout.strip().splitlines():
            parts = line.split()
            if len(parts) >= 2 and parts[1].startswith("refs/tags/"):
                ref = parts[1].replace("refs/tags/", "")
                if not ref.endswith("^{}"):
                    tags.append(ref)
        if not tags:
            return None

        # Sort tags by semver-like comparison
        def semver_sort_key(t: str):
            clean = re.sub(r"^[vV]", "", t)
            num_parts = []
            for piece in clean.split("."):
                num = re.findall(r"^\d+", piece)
                num_parts.append(int(num[0]) if num else 0)
            return tuple(num_parts)

        sorted_tags = sorted(tags, key=semver_sort_key)
        return sorted_tags[-1] if sorted_tags else None
    except Exception:
        return None


def parse_manifest_dependencies(manifest_path: Path) -> dict[str, dict]:
    """Parse dependencies from an idf_component.yml file preserving metadata."""
    if not manifest_path.exists():
        return {}

    lines = manifest_path.read_text(encoding="utf-8").splitlines()
    in_dependencies = False
    deps: dict[str, dict] = {}
    current_dep = None

    for idx, line in enumerate(lines):
        stripped = line.strip()
        # Skip commented lines
        if stripped.startswith("#"):
            continue

        if re.match(r"^dependencies:\s*$", line):
            in_dependencies = True
            continue

        if in_dependencies:
            # Check if out of dependencies block (new root key)
            if re.match(r"^[^\s#]", line):
                break

            # Inline dependency: "  component_name: version_spec"
            inline_match = re.match(r"^ {2}([\w\-/]+):\s*(['\"]?[^'\"#\s]+['\"]?)\s*(?:#.*)?$", line)
            if inline_match:
                dep_name = inline_match.group(1).strip()
                version_raw = inline_match.group(2).strip().strip("'\"")
                if dep_name == "idf":
                    continue
                deps[dep_name] = {
                    "type": "inline",
                    "line_idx": idx,
                    "version_req": version_raw,
                    "indent": 2,
                }
                current_dep = None
                continue

            # Block dependency start: "  component_name:"
            block_start_match = re.match(r"^ {2}([\w\-/]+):\s*$", line)
            if block_start_match:
                dep_name = block_start_match.group(1).strip()
                if dep_name == "idf":
                    current_dep = None
                    continue
                current_dep = dep_name
                deps[current_dep] = {
                    "type": "block",
                    "start_line": idx,
                    "fields": {},
                }
                continue

            # Sub-properties inside block: "    git: ..." or "    version: ..."
            if current_dep:
                field_match = re.match(r"^ {4}([\w\-_]+):\s*(['\"]?.*?['\"]?)\s*(?:#.*)?$", line)
                if field_match:
                    field_key = field_match.group(1).strip()
                    field_val = field_match.group(2).strip().strip("'\"")
                    deps[current_dep]["fields"][field_key] = {
                        "line_idx": idx,
                        "value": field_val,
                    }

    return deps


def update_manifest_file(
    manifest_path: Path,
    dep_name: str,
    dep_info: dict,
    new_version: str,
) -> None:
    """Update a specific dependency's version in the manifest file."""
    lines = manifest_path.read_text(encoding="utf-8").splitlines()

    if dep_info["type"] == "inline":
        line_idx = dep_info["line_idx"]
        prefix_match = re.match(r"^(\s*[\w\-/]+:\s*)", lines[line_idx])
        prefix = prefix_match.group(1) if prefix_match else f"  {dep_name}: "
        lines[line_idx] = f"{prefix}{new_version}"
    elif dep_info["type"] == "block":
        if "version" in dep_info["fields"]:
            line_idx = dep_info["fields"]["version"]["line_idx"]
            prefix_match = re.match(r"^(\s*version:\s*)", lines[line_idx])
            prefix = prefix_match.group(1) if prefix_match else "    version: "
            lines[line_idx] = f"{prefix}\"{new_version}\""

    manifest_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def clean_and_build_project(proj_dir: Path, runner: list[str]) -> bool:
    """Perform a clean build with live verbose output."""
    build_dir = proj_dir / "build"
    sdkconfig_path = proj_dir / "sdkconfig"

    print(f"\n[CLEAN] Purging {build_dir} and {sdkconfig_path}...")
    if build_dir.exists():
        shutil.rmtree(build_dir, ignore_errors=True)
    if sdkconfig_path.exists():
        sdkconfig_path.unlink()

    print(f"[RECONFIGURE & BUILD] Running in {proj_dir}...")
    if runner[0] == "eim":
        cmd = ["eim", "run", "idf.py fullclean build", "v5.5.5"]
    else:
        cmd = ["idf.py", "fullclean", "build"]

    res = subprocess.run(cmd, cwd=proj_dir)
    return res.returncode == 0


def sync_readme_dependency_matrix() -> None:
    """Inspect all idf_component.yml manifests and update the table in README.md."""
    readme_path = REPO_ROOT / "README.md"
    if not readme_path.exists():
        return

    readme_content = readme_path.read_text(encoding="utf-8")

    # Match the Global External Dependency Matrix table
    pattern = r"(## 📦 Global External Dependency Matrix\s*\n\s*The table below summarizes[^\n]*\n\s*\| Sub-Project \| External Dependency \| Locally Installed Version \| Version Requirement \|\n\| :--- \| :--- \| :--- \| :--- \|\n)(.*?)(?=\n---|\n## )"
    match = re.search(pattern, readme_content, re.DOTALL)
    if not match:
        return

    prefix_header = match.group(1)
    existing_table = match.group(2).strip().splitlines()

    # Reconstruct records
    new_rows = []
    for row in existing_table:
        parts = [p.strip() for p in row.split("|")[1:-1]]
        if len(parts) == 4:
            sub_proj, ext_dep, installed_ver, ver_req = parts
            # Check if this sub_proj has an updated manifest
            matched_cfg = next((c for c in PROJECT_CONFIGS if c["project_dir"].name == sub_proj or sub_proj.startswith(c["project_dir"].name) or c["project_dir"].name in sub_proj), None)
            if matched_cfg and matched_cfg["manifest"].exists():
                manifest_deps = parse_manifest_dependencies(matched_cfg["manifest"])
                # Extract clean dep name
                clean_dep = ext_dep.strip("`")
                if clean_dep in manifest_deps:
                    d = manifest_deps[clean_dep]
                    if d["type"] == "inline":
                        ver_req = f"`{d['version_req']}`"
                    elif d["type"] == "block" and "version" in d["fields"]:
                        ver_req = f"`{d['fields']['version']['value']}`"
            new_rows.append(f"| {sub_proj} | {ext_dep} | {installed_ver} | {ver_req} |")

    new_section = prefix_header + "\n".join(new_rows) + "\n"
    updated_readme = readme_content[:match.start()] + new_section + readme_content[match.end():]
    readme_path.write_text(updated_readme, encoding="utf-8")
    print("\n[DOC SYNC] Updated Global External Dependency Matrix in README.md successfully.")


def main():
    parser = argparse.ArgumentParser(
        description="""
================================================================================
  Interactive ESP-IDF Component Dependency Manager (VX Binocle)
================================================================================
Scans, updates, and verifies dependencies across all factory and main applications.
""",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--check-only",
        action="store_true",
        help="Only check for available dependency updates without prompting or modifying files.",
    )
    parser.add_argument(
        "--non-interactive",
        action="store_true",
        help="Automatically apply all recommended version bumps without prompting.",
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="Clean and compile projects after updating dependencies.",
    )
    parser.add_argument(
        "--target-app",
        type=str,
        default=None,
        help="Limit dependency check/update to a specific project name (e.g. interface_board).",
    )

    args = parser.parse_args()
    runner = detect_build_runner()

    print("=" * 70)
    print("  VX Binocle ESP-IDF Dependency Manager")
    print("=" * 70)
    print(f"Runner detected: {' '.join(runner)}")
    print(f"Mode:            {'Check-Only' if args.check_only else 'Interactive Upgrade'}")
    print("=" * 70)

    modified_projects: set[str] = set()

    for proj in PROJECT_CONFIGS:
        if args.target_app and args.target_app.lower() not in proj["key"] and args.target_app.lower() not in proj["project_dir"].name.lower():
            continue

        manifest = proj["manifest"]
        if not manifest.exists():
            continue

        deps = parse_manifest_dependencies(manifest)
        if not deps:
            continue

        print(f"\n📂 Checking {proj['display_name']} ({proj['project_dir'].name}):")
        print("-" * 70)

        for dep_name, dep_info in deps.items():
            # Skip local path components
            if dep_info["type"] == "block" and "path" in dep_info.get("fields", {}):
                continue

            current_spec = ""
            latest_upstream = None

            if dep_info["type"] == "inline":
                current_spec = dep_info["version_req"]
                # Query Espressif registry
                latest_upstream = query_registry_latest_version(dep_name)
            elif dep_info["type"] == "block":
                fields = dep_info.get("fields", {})
                if "git" in fields:
                    git_url = fields["git"]["value"]
                    current_spec = fields.get("version", {}).get("value", "HEAD")
                    latest_upstream = query_git_remote_latest_tag(git_url)
                elif "version" in fields:
                    current_spec = fields["version"]["value"]
                    latest_upstream = query_registry_latest_version(dep_name)

            if not latest_upstream:
                status = "Up-to-date or Unversioned"
                print(f"  • {dep_name:<30} Current: {current_spec:<15} [No remote upgrade found]")
                continue

            # Check if upgrade available
            clean_curr = re.sub(r"^[\^~>=<vV]+", "", current_spec)
            clean_up = re.sub(r"^[\^~>=<vV]+", "", latest_upstream)

            if clean_curr == clean_up or current_spec == "*":
                print(f"  • {dep_name:<30} Current: {current_spec:<15} Latest: {latest_upstream:<10} (Latest)")
                continue

            print(f"  ⚡ {dep_name:<30} Current: {current_spec:<15} Latest: {latest_upstream:<10} [UPDATE AVAILABLE]")

            if args.check_only:
                continue

            # Determine proposed format
            if current_spec.startswith("^"):
                proposed = f"^{clean_up}"
            elif current_spec.startswith("~"):
                proposed = f"~{clean_up}"
            elif current_spec.startswith("v") or current_spec.startswith("V"):
                proposed = f"v{clean_up}"
            else:
                proposed = latest_upstream

            should_update = False
            chosen_version = proposed

            if args.non_interactive:
                should_update = True
            else:
                try:
                    prompt = input(f"    --> Upgrade '{dep_name}' to '{proposed}'? [y/N/custom]: ").strip()
                    if prompt.lower() in ["y", "yes"]:
                        should_update = True
                    elif prompt.lower() not in ["n", "no", ""]:
                        should_update = True
                        chosen_version = prompt
                except EOFError:
                    should_update = False

            if should_update:
                update_manifest_file(manifest, dep_name, dep_info, chosen_version)
                print(f"    [MODIFIED] Updated {manifest.name}: '{dep_name}' -> '{chosen_version}'")
                modified_projects.add(proj["key"])

    # If any manifests were modified, sync docs
    if modified_projects:
        sync_readme_dependency_matrix()

        if args.build or not args.non_interactive:
            build_choice = args.build
            if not build_choice and not args.non_interactive:
                try:
                    b_prompt = input("\nWould you like to clean, reconfigure and build the modified projects now? [y/N]: ").strip()
                    build_choice = b_prompt.lower() in ["y", "yes"]
                except EOFError:
                    build_choice = False

            if build_choice:
                print("\n" + "=" * 70)
                print("  Reconfiguring, Cleaning, and Building Modified Projects")
                print("=" * 70)
                for proj in PROJECT_CONFIGS:
                    if proj["key"] in modified_projects:
                        success = clean_and_build_project(proj["project_dir"], runner)
                        if success:
                            print(f"\n[BUILD SUCCESS] {proj['display_name']} built successfully.")
                        else:
                            print(f"\n[BUILD FAILED] {proj['display_name']} build encountered errors.", file=sys.stderr)
    else:
        print("\nAll scanned project dependencies are current.")


if __name__ == "__main__":
    main()
