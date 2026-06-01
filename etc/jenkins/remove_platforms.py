#!/usr/bin/env python3
"""
filter_platforms.py

Keep only the test‑suite entries whose platform matches one (or more)
platforms supplied on the command line **and** update the environment
section so that it lists only those platforms.

Usage:
    python3 filter_platforms.py -p lp_em_cc2340r5 \
        --input twister-out/testplan.json --output twister-out/filtered_testplan.json

If `---output` is omitted the input file is overwritten (a backup
<filename>.bak is created first).
"""

import argparse
import json
import pathlib
import shutil
import sys
from typing import List


# ----------------------------------------------------------------------
# Helper functions
# ----------------------------------------------------------------------
def load_json(path: pathlib.Path) -> dict:
    """Read a JSON file and return the parsed object."""
    try:
        with path.open("r", encoding="utf-8") as f:
            return json.load(f)
    except Exception as exc:          # pragma: no cover
        sys.exit(f"Failed to read {path}: {exc}")


def save_json(data: dict, path: pathlib.Path, backup: bool = True) -> None:
    """Write JSON data to *path*. Optionally create a backup of the original."""
    if backup and path.exists():
        backup_path = path.with_suffix(path.suffix + ".bak")
        shutil.copy2(path, backup_path)
        print(f"Backup of original file created at: {backup_path}")

    # Keep the same pretty‑print style Twister uses
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=4)
    print(f"Filtered testplan written to: {path}")


# ----------------------------------------------------------------------
# Core filtering logic
# ----------------------------------------------------------------------
def filter_testsuites(data: dict, platforms: List[str]) -> dict:
    """
    Keep only the testsuites whose platform field matches one of *platforms*.
    """
    original_count = len(data.get("testsuites", []))
    filtered = []
    for ts in data.get("testsuites", []):
        platform = ts.get("platform")
        if platform.replace("/", "_")  in platforms:
            filtered.append(ts)
            with open("twister_platform.txt", "w") as f:
                f.write(platform)
            print(f"Wrote {platform} to twister_platform.txt")

    kept = len(filtered)
    removed = original_count - kept

    data["testsuites"] = filtered
    print(f"Kept {kept} testsuite(s) matching platforms: {platforms}")
    print(f"Removed {removed} testsuite(s) that did not match.")
    return data


def filter_environment(data: dict,platforms: List[str]) -> dict:
    """
    Trim the ``environment.options.platform`` list so that it contains only the
    platforms we are keeping.  If the key hierarchy does not exist we simply
    ignore it – the script will still work on the test‑suite list.
    """
    try:
        env_opts = data["environment"]["options"]
        if "platform" in env_opts and isinstance(env_opts["platform"], list):
            original = env_opts["platform"]
            env_opts["platform"] = [p for p in original if p.replace("/", "_") in platforms]
            print(
                f"Environment platforms reduced from {len(original)} to "
                f"{len(env_opts['platform'])} entries."
            )
    except KeyError:
        # The file may not have an ``environment`` section (unlikely for Twister
        # output).  We silently skip the step.
        pass
    return data


# ----------------------------------------------------------------------
# Argument handling
# ----------------------------------------------------------------------
def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Keep only testsuites for specific platforms in a Twister testplan "
            "and update the environment.platform list accordingly."
        ),
        allow_abbrev=False
    )
    parser.add_argument(
        "--platform",
        action="append",
        required=True,
        help="Platform to keep. Can be given multiple times.",
    )
    parser.add_argument(
        "--input",
        type=pathlib.Path,
        required=True,
        help="Path to the original testplan json file.",
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        help=(
            "Path to write the filtered testplan. If omitted, the input file is "
            "overwritten (a backup is created)."
        ),
    )
    return parser.parse_args()


# ----------------------------------------------------------------------
# Main entry point
# ----------------------------------------------------------------------
def main() -> None:
    args = parse_args()

    if not args.input.is_file():
        sys.exit(f"Input file does not exist: {args.input}")

    # Load the original JSON
    data = load_json(args.input)

    # 1 Keep only the desired testsuites
    data = filter_testsuites(data, args.platform)

    # 2️ Trim the environment.platform list
    data = filter_environment(data, args.platform)

    # 3 Write the result (backup if we are overwriting the original)
    out_path = args.output if args.output else args.input
    save_json(data, out_path, backup=out_path == args.input)


if __name__ == "__main__":
    main()
