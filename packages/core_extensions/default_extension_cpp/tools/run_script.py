#!/usr/bin/env python3
#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import argparse
import platform
import subprocess
import sys
import os

BUILD_TYPE = "debug"


def detect_os() -> str:
    """Detect operating system in the format needed for tgn."""
    system = platform.system().lower()

    if system == "linux":
        return "linux"

    if system == "darwin":
        return "mac"

    if system == "windows":
        return "win"

    raise RuntimeError(f"Unsupported OS: {system}")


def detect_arch() -> str:
    """Detect architecture in the format needed for tgn."""
    machine = platform.machine().lower()

    if machine in ("x86_64", "amd64"):
        return "x64"

    if machine in ("i386", "i686", "x86"):
        return "x86"

    if machine in ("arm64", "aarch64"):
        return "arm64"

    if machine.startswith("arm"):
        return "arm"

    raise RuntimeError(f"Unsupported architecture: {machine}")


def run_cmd(cmd: str, env: dict[str, str] | None = None) -> int:
    """Run a shell command."""
    if env is None:
        env = os.environ.copy()
    print(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True, check=True, env=env)
    return result.returncode


def run_cmd_test(os_str: str, _arch: str) -> int:
    """Start the application."""
    env = os.environ.copy()

    if os_str == "win":
        env["PATH"] = (
            env["PATH"] + ";" + ".ten/app/ten_packages/system/ten_runtime/lib"
        )
        command = "bin/default_extension_cpp_test.exe"
    else:
        command = "bin/default_extension_cpp_test"

    return run_cmd(command, env)


def run_cmd_build(os: str, arch: str) -> int:
    """Build the application."""
    if os == "win":
        command = (
            f"tgn.bat gen {os} {arch} {BUILD_TYPE} "
            "-- ten_enable_standalone_test=true"
        )
    else:
        command = (
            f"tgn gen {os} {arch} {BUILD_TYPE} "
            "-- ten_enable_standalone_test=true"
        )

    rc = run_cmd(command)
    if rc != 0:
        return rc

    if os == "win":
        command = f"tgn.bat build {os} {arch} {BUILD_TYPE}"
    else:
        command = f"tgn build {os} {arch} {BUILD_TYPE}"

    return run_cmd(command)


def main():
    parser = argparse.ArgumentParser(
        description="Run scripts based on manifest.json"
    )
    parser.add_argument(
        "cmd", choices=["test", "build"], help="Command to execute"
    )

    args = parser.parse_args()

    # Detect OS and architecture.
    current_os = detect_os()
    current_arch = detect_arch()

    # Handle the command based on platform.
    rc = 0

    if args.cmd == "test":
        rc = run_cmd_test(current_os, current_arch)
    elif args.cmd == "build":
        rc = run_cmd_build(current_os, current_arch)

    sys.exit(rc)


if __name__ == "__main__":
    main()
