#!/usr/bin/env python3

import argparse
import json
import subprocess
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create a single merged ESP-IDF firmware bin for web flashing."
    )
    parser.add_argument(
        "--build-dir",
        default="build",
        help="ESP-IDF build directory (default: build)",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output merged .bin path (default: <build-dir>/web_flasher_combined.bin)",
    )
    parser.add_argument(
        "--flasher-args",
        default=None,
        help="Path to flasher_args.json (default: <build-dir>/flasher_args.json)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    build_dir = Path(args.build_dir).resolve()
    flasher_args_path = (
        Path(args.flasher_args).resolve()
        if args.flasher_args
        else (build_dir / "flasher_args.json")
    )
    output_path = (
        Path(args.output).resolve()
        if args.output
        else (build_dir / "web_flasher_combined.bin")
    )

    if not flasher_args_path.exists():
        print(
            f"Error: flasher args file not found: {flasher_args_path}\n"
            "Run 'idf.py build' first.",
            file=sys.stderr,
        )
        return 1

    with flasher_args_path.open("r", encoding="utf-8") as handle:
        flasher_args = json.load(handle)

    chip = flasher_args.get("extra_esptool_args", {}).get("chip", "esp32p4")
    write_flash_args = flasher_args.get("write_flash_args", [])
    flash_files = flasher_args.get("flash_files", {})

    if not flash_files:
        print(
            "Error: no flash files found in flasher_args.json. Run 'idf.py build' first.",
            file=sys.stderr,
        )
        return 1

    output_path.parent.mkdir(parents=True, exist_ok=True)

    command = [
        sys.executable,
        "-m",
        "esptool",
        "--chip",
        chip,
        "merge_bin",
        "-o",
        str(output_path),
    ]
    command.extend(write_flash_args)

    for offset in sorted(flash_files.keys(), key=lambda item: int(item, 16)):
        flash_file = build_dir / flash_files[offset]
        if not flash_file.exists():
            print(
                f"Error: required flash image not found: {flash_file}",
                file=sys.stderr,
            )
            return 1
        command.extend([offset, str(flash_file)])

    print("Running:")
    print(" ".join(command))
    subprocess.run(command, check=True)
    print(f"Created merged binary: {output_path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        print(f"Error: merge_bin failed with exit code {error.returncode}", file=sys.stderr)
        raise SystemExit(error.returncode)