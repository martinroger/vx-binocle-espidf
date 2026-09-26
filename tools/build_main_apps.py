#!/usr/bin/env python3
"""
Automated Main Cluster Applications Builder (VX Binocle)
=========================================================
Builds the primary firmware applications for the VX Binocle instrument cluster:
  - Interface Board (ITF)       ==> interface_board.bin
  - Left Display Board (LDB)     ==> left_screen.bin
  - Right Display Board (RDB)    ==> right_screen.bin

Strictly pins to ESP-IDF v5.5.5 when running via eim.
Supports interactive step-by-step guidance when invoked without arguments.

Usage:
    python tools/build_main_apps.py [--app {itf,ldb,left,rdb,right,all}] [--output-dir DIR] [--clean] [--flash-bundle]
"""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

# Workspace Root
REPO_ROOT = Path(__file__).resolve().parent.parent

# Pinned ESP-IDF Framework Version
TARGET_IDF_VERSION = "v5.5.5"

# Main target configurations
TARGET_CONFIGS = {
    "itf": {
        "name": "ITF",
        "display_name": "Interface Board",
        "project_dir": REPO_ROOT / "interface_board",
        "app_bin": "interface_board.bin",
        "aliases": ["itf", "interface_board"],
    },
    "left": {
        "name": "LDB",
        "display_name": "Left Display Board",
        "project_dir": REPO_ROOT / "left_screen",
        "app_bin": "left_screen.bin",
        "aliases": ["left", "ldb", "left_screen"],
    },
    "right": {
        "name": "RDB",
        "display_name": "Right Display Board",
        "project_dir": REPO_ROOT / "right_screen",
        "app_bin": "right_screen.bin",
        "aliases": ["right", "rdb", "right_screen"],
    },
}


def calculate_md5(file_path: Path) -> str:
    """Calculate MD5 checksum of a file."""
    hasher = hashlib.md5()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def human_size(num_bytes: int) -> str:
    """Format bytes into a human readable string."""
    for unit in ["B", "KB", "MB", "GB"]:
        if num_bytes < 1024.0:
            return f"{num_bytes:.1f} {unit}"
        num_bytes /= 1024.0
    return f"{num_bytes:.1f} TB"


def detect_build_runner() -> list[str]:
    """Detect whether to use eim or idf.py."""
    if shutil.which("eim"):
        return ["eim", "run"]
    if shutil.which("idf.py"):
        return ["idf.py"]
    return ["idf.py"]


def run_command(cmd: list[str], cwd: Path, description: str) -> None:
    """Run a shell command with proper error reporting."""
    print(f"\n[EXEC] {description}")
    print(f"       Directory: {cwd}")
    print(f"       Command:   {' '.join(cmd)}")

    res = subprocess.run(cmd, cwd=cwd)
    if res.returncode != 0:
        print(f"\n[ERROR] Command failed with exit code {res.returncode}: {' '.join(cmd)}", file=sys.stderr)
        sys.exit(res.returncode)


def build_main_project(
    target_key: str,
    runner: list[str],
    output_dir: Path,
    clean: bool = False,
    flash_bundle: bool = False,
) -> tuple[Path, str, str]:
    """Build a main application project and copy output binary (or full bundle) to output_dir."""
    cfg = TARGET_CONFIGS[target_key]
    proj_dir = cfg["project_dir"]
    build_dir = proj_dir / "build"

    print("\n" + "=" * 60)
    print(f"  Building Main App: {cfg['display_name']} ({cfg['name']}) [IDF: {TARGET_IDF_VERSION}]")
    print("=" * 60)

    if clean:
        if build_dir.exists():
            print(f"[CLEAN] Removing {build_dir}")
            shutil.rmtree(build_dir, ignore_errors=True)
        sdkconfig_path = proj_dir / "sdkconfig"
        if sdkconfig_path.exists():
            print(f"[CLEAN] Removing {sdkconfig_path}")
            sdkconfig_path.unlink()

    if runner[0] == "eim":
        # Strictly pin to TARGET_IDF_VERSION to prevent context leakage
        cmd = ["eim", "run", "idf.py build", TARGET_IDF_VERSION]
    else:
        cmd = ["idf.py", "build"]

    run_command(cmd, cwd=proj_dir, description=f"Building {cfg['display_name']}")

    app_bin_path = build_dir / cfg["app_bin"]
    if not app_bin_path.exists():
        print(f"[ERROR] Expected app binary not found: {app_bin_path}", file=sys.stderr)
        sys.exit(1)

    dest_app_bin = output_dir / cfg["app_bin"]
    shutil.copy2(app_bin_path, dest_app_bin)

    if flash_bundle:
        bundle_dir = output_dir / f"{cfg['name'].lower()}_bundle"
        bundle_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(app_bin_path, bundle_dir / cfg["app_bin"])

        bootloader_bin = build_dir / "bootloader" / "bootloader.bin"
        partition_bin = build_dir / "partition_table" / "partition-table.bin"
        flasher_args = build_dir / "flasher_args.json"

        if bootloader_bin.exists():
            shutil.copy2(bootloader_bin, bundle_dir / "bootloader.bin")
        if partition_bin.exists():
            shutil.copy2(partition_bin, bundle_dir / "partition-table.bin")
        if flasher_args.exists():
            shutil.copy2(flasher_args, bundle_dir / "flasher_args.json")

        print(f"[BUNDLE] Exported full flashing bundle to: {bundle_dir}")

    md5_hash = calculate_md5(dest_app_bin)
    size_str = human_size(dest_app_bin.stat().st_size)

    return dest_app_bin, size_str, md5_hash


def resolve_target(app_arg: str) -> list[str]:
    """Resolve CLI target argument to list of target keys."""
    app_lower = app_arg.lower()
    if app_lower == "all":
        return ["itf", "left", "right"]

    for key, cfg in TARGET_CONFIGS.items():
        if app_lower == key or app_lower in cfg["aliases"]:
            return [key]

    valid = ["all"] + [alias for cfg in TARGET_CONFIGS.values() for alias in cfg["aliases"]]
    print(f"[ERROR] Unknown application '{app_arg}'. Valid choices: {', '.join(valid)}", file=sys.stderr)
    sys.exit(1)


def interactive_wizard() -> tuple[list[str], bool, bool, Path]:
    """Interactively guide the user to select what to build and how."""
    print("=" * 65)
    print("  🚀 VX Binocle Main Apps Builder — Interactive Wizard")
    print(f"  Target Framework: ESP-IDF {TARGET_IDF_VERSION}")
    print("=" * 65)
    print("\nSelect applications to build:")
    print("  [1] All 3 cluster apps (Interface Board, Left Display, Right Display)")
    print("  [2] Interface Board only (ITF)")
    print("  [3] Left Display Board only (LDB)")
    print("  [4] Right Display Board only (RDB)")
    print("  [5] Custom multi-selection (step-by-step)")

    target_keys = ["itf", "left", "right"]
    choice = input("\nEnter choice [1-5] (default 1): ").strip()
    if choice == "2":
        target_keys = ["itf"]
    elif choice == "3":
        target_keys = ["left"]
    elif choice == "4":
        target_keys = ["right"]
    elif choice == "5":
        target_keys = []
        for key in ["itf", "left", "right"]:
            cfg = TARGET_CONFIGS[key]
            ans = input(f"  Build {cfg['display_name']} ({cfg['name']})? [y/N]: ").strip().lower()
            if ans in ["y", "yes"]:
                target_keys.append(key)
        if not target_keys:
            print("[INFO] No applications selected. Defaulting to all.")
            target_keys = ["itf", "left", "right"]
    else:
        target_keys = ["itf", "left", "right"]

    clean_ans = input("\nPerform clean build (purge build/ directory and root sdkconfig)? [y/N]: ").strip().lower()
    clean = clean_ans in ["y", "yes"]

    bundle_ans = input("Export full flashing bundle (bootloader, partition-table, args)? [y/N]: ").strip().lower()
    flash_bundle = bundle_ans in ["y", "yes"]

    out_ans = input(f"Output directory (default: dist/main_apps): ").strip()
    output_dir = Path(out_ans).resolve() if out_ans else (REPO_ROOT / "dist" / "main_apps")

    return target_keys, clean, flash_bundle, output_dir


def main():
    # If invoked with no CLI arguments, launch interactive wizard
    is_interactive_flow = len(sys.argv) == 1

    parser = argparse.ArgumentParser(
        description=f"""
================================================================================
  Automated Main Cluster Applications Builder (VX Binocle)
================================================================================
Builds the primary instrument cluster firmware applications:
  1. ITF   -> Interface Board       ==> interface_board.bin
  2. LDB   -> Left Display Board    ==> left_screen.bin
  3. RDB   -> Right Display Board   ==> right_screen.bin

Strictly pins build execution to ESP-IDF {TARGET_IDF_VERSION}.
""",
        epilog="""
EXAMPLES:
  Build all 3 cluster main applications:
    python tools/build_main_apps.py --app all

  Build only the Left Display:
    python tools/build_main_apps.py --app left

  Perform clean build and export full flash bundles:
    python tools/build_main_apps.py --app all --clean --flash-bundle --output-dir dist/main_release
""",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--app",
        default="all",
        help=(
            "Target application to compile:\n"
            "  'itf' / 'interface_board': Interface Board\n"
            "  'left' / 'ldb' / 'left_screen': Left Display Board\n"
            "  'right' / 'rdb' / 'right_screen': Right Display Board\n"
            "  'all': All three applications sequentially (default)"
        ),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "dist" / "main_apps",
        help="Destination directory for binaries and checksums.md5 (default: dist/main_apps)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Perform clean build by deleting target build directory and root sdkconfig.",
    )
    parser.add_argument(
        "--flash-bundle",
        action="store_true",
        help="Export full flashing bundle (bootloader.bin, partition-table.bin, flasher_args.json) in addition to app.bin.",
    )

    runner = detect_build_runner()

    if is_interactive_flow:
        targets, clean, flash_bundle, output_dir = interactive_wizard()
    else:
        args = parser.parse_args()
        targets = resolve_target(args.app)
        clean = args.clean
        flash_bundle = args.flash_bundle
        output_dir = args.output_dir.resolve()

    output_dir.mkdir(parents=True, exist_ok=True)

    print("\n" + "=" * 65)
    print("  VX Binocle Main Applications Build Tool")
    print("=" * 65)
    print(f"Runner:       {' '.join(runner)} (Pinned: {TARGET_IDF_VERSION})")
    print(f"Targets:      {', '.join([TARGET_CONFIGS[t]['name'] for t in targets])}")
    print(f"Output Dir:   {output_dir}")
    print(f"Clean Build:  {clean}")
    print(f"Flash Bundle: {flash_bundle}")
    print("=" * 65)

    generated_apps: list[tuple[str, Path, str, str]] = []

    for target_key in targets:
        cfg = TARGET_CONFIGS[target_key]
        dest_bin, size_str, md5_hash = build_main_project(
            target_key=target_key,
            runner=runner,
            output_dir=output_dir,
            clean=clean,
            flash_bundle=flash_bundle,
        )
        generated_apps.append((cfg["name"], dest_bin, size_str, md5_hash))

    # Write MD5 checksum file
    checksum_file = output_dir / "checksums.md5"
    with open(checksum_file, "w") as f:
        for name, bin_path, size_str, md5_hash in generated_apps:
            f.write(f"{md5_hash}  {bin_path.name}\n")

    print("\n" + "=" * 65)
    print("  🎉 Main Applications Build Complete!")
    print("=" * 65)
    print(f"{'Target':<10} {'Filename':<25} {'Size':<12} {'MD5 Checksum'}")
    print("-" * 65)
    for name, bin_path, size_str, md5_hash in generated_apps:
        print(f"{name:<10} {bin_path.name:<25} {size_str:<12} {md5_hash}")
    print("-" * 65)
    print(f"Checksums saved to: {checksum_file}\n")


if __name__ == "__main__":
    main()
