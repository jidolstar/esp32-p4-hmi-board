"""sample-suite runtime font를 준비하고 HMI SD 자산을 갱신한다."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TOOLS_ROOT = REPOSITORY_ROOT / "tools"
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from assets.push import (  # noqa: E402
    AssetPushError,
    cancel_preparation,
    set_preparation,
)
from hmi_project.commands import push_assets  # noqa: E402
from hmi_project.generate import GenerationError  # noqa: E402
from hmi_project.manifest import ManifestError, load_registry  # noqa: E402


PROFILE_ID = "sample-suite"
FONT_CONFIG = REPOSITORY_ROOT / "samples/suite/i18n/catalog.json"
FONT_GENERATOR = REPOSITORY_ROOT / "tools/assets/typography/generate_user.py"


def _ensure_fonts() -> int:
    completed = subprocess.run(
        [
            sys.executable,
            str(FONT_GENERATOR),
            "--repository-root",
            str(REPOSITORY_ROOT),
            "--config",
            str(FONT_CONFIG),
            "--ensure",
        ],
        cwd=REPOSITORY_ROOT,
        check=False,
    )
    return completed.returncode


def _best_effort_cancel(port: str) -> None:
    try:
        cancel_preparation(port, PROFILE_ID)
    except AssetPushError as error:
        print(f"warning: HMI preparation screen could not be cancelled: {error}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args(argv)

    try:
        registry = load_registry(REPOSITORY_ROOT)
        if args.list_ports:
            return push_assets(
                REPOSITORY_ROOT,
                registry,
                PROFILE_ID,
                None,
                dry_run=False,
                list_ports=True,
            )
        if not args.dry_run and args.port is None:
            parser.error("--port is required unless --dry-run or --list-ports is used")

        preparation_started = False
        if not args.dry_run:
            print("[1/3] Opening the HMI update screen...", flush=True)
            set_preparation(args.port, PROFILE_ID, "fonts")
            preparation_started = True
        print("[2/3] Checking and generating runtime fonts...", flush=True)
        if _ensure_fonts() != 0:
            if preparation_started:
                _best_effort_cancel(args.port)
            return 1

        if preparation_started:
            set_preparation(args.port, PROFILE_ID, "assets")
        print("[3/3] Bundling, verifying, and transferring assets...", flush=True)
        result = push_assets(
            REPOSITORY_ROOT,
            registry,
            PROFILE_ID,
            args.port,
            dry_run=args.dry_run,
            list_ports=False,
        )
        if result != 0 and preparation_started:
            _best_effort_cancel(args.port)
        return result
    except (AssetPushError, GenerationError, ManifestError) as error:
        if args.port and not args.dry_run:
            _best_effort_cancel(args.port)
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
