#!/usr/bin/env python3
"""
Automated Bennu Delivery Package Generator
=========================================
Builds factory applications and wraps them into Bennu "special delivery" packages
for OTA deployment.

Usage:
    python tools/build_deliveries.py [--app {itf,ldb,rdb,all}] [--output-dir DIR] [--from-bins DIR]
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

# Workspace Root
REPO_ROOT = Path(__file__).resolve().parent.parent

# Factory target configurations
TARGET_CONFIGS = {
    "itf": {
        "name": "ITF",
        "display_name": "ITF Factory App",
        "project_dir": REPO_ROOT / "factory apps" / "ITF factory app",
        "app_bin": "ITF_factory.bin",
        "storage_bin": "storage.bin",
        "output_bennu_bin": "ITF_bennu.bin",
    },
    "ldb": {
        "name": "LDB",
        "display_name": "Left Display Factory App",
        "project_dir": REPO_ROOT / "factory apps" / "left display factory app",
        "app_bin": "LDB_factory.bin",
        "storage_bin": "storage.bin",
        "output_bennu_bin": "LDB_bennu.bin",
    },
    "rdb": {
        "name": "RDB",
        "display_name": "Right Display Factory App",
        "project_dir": REPO_ROOT / "factory apps" / "right display factory app",
        "app_bin": "RDB_factory.bin",
        "storage_bin": "storage.bin",
        "output_bennu_bin": "RDB_bennu.bin",
    },
}

BENNU_DIR = REPO_ROOT / "bennu"


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


def build_factory_project(target_key: str, runner: list[str], clean: bool = False) -> tuple[Path, Path]:
    """Build a factory application project and return paths to app.bin and storage.bin."""
    cfg = TARGET_CONFIGS[target_key]
    proj_dir = cfg["project_dir"]
    build_dir = proj_dir / "build"

    print(f"\n========================================================")
    print(f"  Building Factory App: {cfg['display_name']} ({cfg['name']})")
    print(f"========================================================")

    if clean and build_dir.exists():
        print(f"[CLEAN] Removing {build_dir}")
        shutil.rmtree(build_dir, ignore_errors=True)

    if runner[0] == "eim":
        cmd = ["eim", "run", "idf.py build"]
    else:
        cmd = ["idf.py", "build"]

    run_command(cmd, cwd=proj_dir, description=f"Building {cfg['display_name']}")

    app_bin_path = build_dir / cfg["app_bin"]
    storage_bin_path = build_dir / cfg["storage_bin"]

    if not app_bin_path.exists():
        print(f"[ERROR] Expected app binary not found: {app_bin_path}", file=sys.stderr)
        sys.exit(1)

    if not storage_bin_path.exists():
        print(f"[ERROR] Expected storage binary not found: {storage_bin_path}", file=sys.stderr)
        sys.exit(1)

    return app_bin_path, storage_bin_path


def build_bennu_delivery(
    target_key: str,
    payload_bin: Path,
    storage_bin: Path,
    output_dir: Path,
    runner: list[str],
    clean: bool = False,
) -> Path:
    """Build Bennu embedding the specific payload and storage binaries."""
    cfg = TARGET_CONFIGS[target_key]
    bennu_build_dir = BENNU_DIR / f"build_{target_key}"
    output_bennu_file = output_dir / cfg["output_bennu_bin"]

    print(f"\n========================================================")
    print(f"  Packaging Bennu Delivery: {cfg['output_bennu_bin']}")
    print(f"========================================================")
    print(f"  Payload: {payload_bin} ({human_size(payload_bin.stat().st_size)})")
    print(f"  Storage: {storage_bin} ({human_size(storage_bin.stat().st_size)})")

    if clean and bennu_build_dir.exists():
        print(f"[CLEAN] Removing {bennu_build_dir}")
        shutil.rmtree(bennu_build_dir, ignore_errors=True)

    bennu_build_dir.mkdir(parents=True, exist_ok=True)
    # Stage payload and storage binaries into target build folder
    staged_payload = bennu_build_dir / "payload.bin"
    staged_storage = bennu_build_dir / "storage.bin"
    shutil.copy2(payload_bin, staged_payload)
    shutil.copy2(storage_bin, staged_storage)

    build_dir_str = str(bennu_build_dir.name)

    if runner[0] == "eim":
        idf_cmd = f"idf.py -B {build_dir_str} build"
        cmd = ["eim", "run", idf_cmd]
    else:
        cmd = [
            "idf.py",
            "-B",
            build_dir_str,
            "build",
        ]

    run_command(cmd, cwd=BENNU_DIR, description=f"Building Bennu delivery package for {cfg['name']}")

    bennu_bin_built = bennu_build_dir / "bennu.bin"
    if not bennu_bin_built.exists():
        print(f"[ERROR] Built Bennu binary not found at: {bennu_bin_built}", file=sys.stderr)
        sys.exit(1)

    output_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(bennu_bin_built, output_bennu_file)
    print(f"[SUCCESS] Generated: {output_bennu_file} ({human_size(output_bennu_file.stat().st_size)})")

    return output_bennu_file


def main():
    parser = argparse.ArgumentParser(
        description="""
================================================================================
  Bennu Special Delivery Package Generator (VX Binocle)
================================================================================
Automates the compilation and packaging of "special delivery" OTA binaries for
the three factory applications:
  1. ITF  -> Interface Board Factory App   ==> ITF_bennu.bin
  2. LDB  -> Left Display Factory App      ==> LDB_bennu.bin
  3. RDB  -> Right Display Factory App     ==> RDB_bennu.bin

HOW IT WORKS:
  - Compiles the selected factory app (or takes pre-built binaries).
  - Collects the factory application binary and its SPIFFS storage image.
  - Stages them into an isolated Bennu build folder (bennu/build_<target>).
  - Compiles Bennu to embed both binaries into a bootable OTA package.
  - Copies the resulting '<TARGET>_bennu.bin' into the output directory and
    generates MD5 checksums for release verification.
""",
        epilog="""
EXAMPLES:
  Build all 3 delivery packages from scratch:
    python tools/build_deliveries.py --app all

  Build only the Interface Board (ITF) delivery package:
    python tools/build_deliveries.py --app itf

  Assemble Bennu packages using pre-existing factory build outputs:
    python tools/build_deliveries.py --from-bins "factory apps/ITF factory app/build" --app itf

  Clean previous build directories and export to custom path:
    python tools/build_deliveries.py --app all --clean --output-dir dist/release_bins

================================================================================
""",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--app",
        choices=["itf", "ldb", "rdb", "all"],
        default="all",
        help=(
            "Target application to compile and wrap with Bennu:\n"
            "  'itf': Interface Board (outputs ITF_bennu.bin)\n"
            "  'ldb': Left Display Board (outputs LDB_bennu.bin)\n"
            "  'rdb': Right Display Board (outputs RDB_bennu.bin)\n"
            "  'all': All three applications sequentially (default)"
        ),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=REPO_ROOT / "dist" / "delivery",
        help=(
            "Destination directory where '<TARGET>_bennu.bin' packages and\n"
            "'checksums.md5' will be saved (default: dist/delivery)"
        ),
    )
    parser.add_argument(
        "--from-bins",
        type=Path,
        default=None,
        metavar="DIR",
        help=(
            "Path to directory containing pre-built factory application and storage\n"
            "binaries. When specified, skips the factory compilation step and packages\n"
            "Bennu directly from existing binaries."
        ),
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Perform a clean build by deleting target build directories prior to compilation.",
    )

    args = parser.parse_args()
    targets = ["itf", "ldb", "rdb"] if args.app == "all" else [args.app]
    runner = detect_build_runner()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("  Bennu Special Delivery Package Automation Tool")
    print("=" * 60)
    print(f"Runner:     {' '.join(runner)}")
    print(f"Targets:    {', '.join(targets)}")
    print(f"Output Dir: {output_dir}")
    if args.from_bins:
        print(f"From Bins:  {args.from_bins.resolve()}")
    print("=" * 60)

    generated_packages: list[tuple[str, Path, str, str]] = []

    for target_key in targets:
        cfg = TARGET_CONFIGS[target_key]

        if args.from_bins:
            from_dir = args.from_bins.resolve()
            app_bin_candidates = [
                from_dir / cfg["app_bin"],
                from_dir / f"{cfg['name']}_factory_app.bin",
                from_dir / f"{cfg['project_dir'].name.replace(' ', '_')}_app.bin",
            ]
            storage_bin_candidates = [
                from_dir / cfg["storage_bin"],
                from_dir / f"{cfg['name']}_factory_storage.bin",
                from_dir / f"{cfg['project_dir'].name.replace(' ', '_')}_storage.bin",
            ]

            payload_bin = next((p for p in app_bin_candidates if p.exists()), None)
            storage_bin = next((p for p in storage_bin_candidates if p.exists()), None)

            if not payload_bin or not storage_bin:
                print(f"[ERROR] Could not find pre-built binaries for {cfg['name']} in {from_dir}", file=sys.stderr)
                if not payload_bin:
                    print(f"        Missing app binary (checked: {[str(c) for c in app_bin_candidates]})", file=sys.stderr)
                if not storage_bin:
                    print(f"        Missing storage binary (checked: {[str(c) for c in storage_bin_candidates]})", file=sys.stderr)
                sys.exit(1)
        else:
            payload_bin, storage_bin = build_factory_project(target_key, runner, clean=args.clean)

        bennu_pkg = build_bennu_delivery(
            target_key=target_key,
            payload_bin=payload_bin,
            storage_bin=storage_bin,
            output_dir=output_dir,
            runner=runner,
            clean=args.clean,
        )

        md5_hash = calculate_md5(bennu_pkg)
        size_str = human_size(bennu_pkg.stat().st_size)
        generated_packages.append((cfg["name"], bennu_pkg, size_str, md5_hash))

    # Write MD5 checksum file
    checksum_file = output_dir / "checksums.md5"
    with open(checksum_file, "a") as f:
        for name, pkg_path, size_str, md5_hash in generated_packages:
            f.write(f"{md5_hash}  {pkg_path.name}\n")

    print("\n" + "=" * 60)
    print("  🎉 Delivery Package Generation Complete!")
    print("=" * 60)
    print(f"{'Target':<10} {'Filename':<25} {'Size':<12} {'MD5 Checksum'}")
    print("-" * 60)
    for name, pkg_path, size_str, md5_hash in generated_packages:
        print(f"{name:<10} {pkg_path.name:<25} {size_str:<12} {md5_hash}")
    print("-" * 60)
    print(f"Checksums saved to: {checksum_file}\n")


if __name__ == "__main__":
    main()
