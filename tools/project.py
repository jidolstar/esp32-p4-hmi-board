#!/usr/bin/env python3
"""HMI starter profile CLI."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from hmi_project.commands import (
    bundle_assets,
    build_profile,
    check_isolation,
    check_profile,
    doctor,
    list_profiles,
    prepare,
    push_assets,
    run_profile,
)
from hmi_project.generate import GenerationError
from hmi_project.manifest import ManifestError, load_registry
from hmi_project.windows_tools import WindowsToolError


def repository_root() -> Path:
    return Path(__file__).resolve().parent.parent


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Validate and inspect HMI build profiles.")
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("list", help="validate and list registered profiles")

    doctor_parser = subparsers.add_parser(
        "doctor", help="run read-only manifest and host tool checks"
    )
    doctor_parser.add_argument("--host", required=True, choices=("windows", "hmi"))
    doctor_parser.add_argument("--profile", required=True)

    prepare_parser = subparsers.add_parser(
        "prepare", help="generate deterministic build inputs for one host/profile"
    )
    prepare_parser.add_argument("--host", required=True, choices=("windows", "hmi"))
    prepare_parser.add_argument("--profile", required=True)

    bundle_parser = subparsers.add_parser(
        "bundle-assets", help="copy declared HMI assets into dist/sd"
    )
    bundle_parser.add_argument("--profile", required=True)

    push_parser = subparsers.add_parser(
        "push-assets", help="bundle and push HMI assets over USB CDC"
    )
    push_parser.add_argument("--profile", required=True)
    push_parser.add_argument("--port")
    push_parser.add_argument("--dry-run", action="store_true")
    push_parser.add_argument("--list-ports", action="store_true")

    check_parser = subparsers.add_parser("check", help="validate generated inputs")
    check_parser.add_argument("--scope", choices=("isolation",))
    check_parser.add_argument("--host", choices=("windows", "hmi"))
    check_parser.add_argument("--profile")

    build_parser = subparsers.add_parser(
        "build", help="prepare and build one host/profile"
    )
    build_parser.add_argument("--host", required=True, choices=("windows", "hmi"))
    build_parser.add_argument("--profile", required=True)

    run_parser = subparsers.add_parser("run", help="build and run a Windows profile")
    run_parser.add_argument("--host", required=True, choices=("windows", "hmi"))
    run_parser.add_argument("--profile", required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = make_parser().parse_args(argv)
    try:
        root = repository_root()
        registry = load_registry(root)
        if args.command == "list":
            return list_profiles(registry)
        if args.command == "doctor":
            return doctor(root, registry.get(args.profile), args.host)
        if args.command == "prepare":
            return prepare(root, registry, args.host, args.profile)
        if args.command == "bundle-assets":
            return bundle_assets(root, registry, args.profile)
        if args.command == "push-assets":
            return push_assets(
                root,
                registry,
                args.profile,
                args.port,
                dry_run=args.dry_run,
                list_ports=args.list_ports,
            )
        if args.command == "check":
            if args.scope == "isolation" and args.host is None and args.profile is None:
                return check_isolation(root, registry)
            if args.scope is None and args.host is not None and args.profile is not None:
                return check_profile(root, registry, args.host, args.profile)
            make_parser().error(
                "check requires either --scope isolation or both --host and --profile"
            )
        if args.command == "build":
            return build_profile(root, registry, args.host, args.profile)
        if args.command == "run":
            return run_profile(root, registry, args.host, args.profile)
    except (ManifestError, GenerationError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    except WindowsToolError as error:
        print(f"error: {error}", file=sys.stderr)
        return 3
    raise AssertionError(f"unhandled command: {args.command}")


if __name__ == "__main__":
    raise SystemExit(main())
