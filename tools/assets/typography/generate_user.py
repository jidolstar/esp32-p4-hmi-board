"""Generate split LVGL binary fonts for bounded dynamic user text rendering."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    from .font_assets import (
        FontAssetError,
        build_user_outputs,
        check_outputs,
        check_no_extra_outputs,
        converter_path,
        load_font_config,
        remove_extra_outputs,
        write_outputs,
    )
    from .owner_config import OwnerConfigError, load_owner_config
except ImportError:  # Direct script execution from the repository root.
    from font_assets import (  # type: ignore[no-redef]
        FontAssetError,
        build_user_outputs,
        check_outputs,
        check_no_extra_outputs,
        converter_path,
        load_font_config,
        remove_extra_outputs,
        write_outputs,
    )
    from owner_config import OwnerConfigError, load_owner_config  # type: ignore[no-redef]


def main(argv: list[str] | None = None) -> int:
    repository_root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="validate without writing")
    mode.add_argument("--check", action="store_true", help="verify generated output bytes")
    mode.add_argument(
        "--ensure",
        action="store_true",
        help="generate only when runtime font outputs are missing or stale",
    )
    parser.add_argument("--repository-root", type=Path, default=repository_root)
    parser.add_argument("--config", type=Path, required=True, help="owner catalog config")
    args = parser.parse_args(argv)

    try:
        owner = load_owner_config(args.config)
        config = load_font_config(args.repository_root, owner.fonts)
        converter_path(args.repository_root)
        if args.dry_run:
            action = "validated"
            outputs = None
        else:
            outputs = build_user_outputs(
                args.repository_root,
                config,
                owner.cpp_namespace,
                owner.user_asset_prefix,
            )
            binary_outputs = {
                name: content for name, content in outputs.items() if name.endswith(".bin")
            }
            metadata_outputs = {
                name: content for name, content in outputs.items() if not name.endswith(".bin")
            }
            if args.check:
                check_outputs(owner.user_output, binary_outputs)
                check_no_extra_outputs(owner.user_output, binary_outputs, "user_*.bin")
                check_outputs(owner.generated_output, metadata_outputs)
                action = "checked"
            elif args.ensure:
                try:
                    check_outputs(owner.user_output, binary_outputs)
                    check_no_extra_outputs(owner.user_output, binary_outputs, "user_*.bin")
                    check_outputs(owner.generated_output, metadata_outputs)
                    action = "ready"
                except FontAssetError:
                    write_outputs(owner.user_output, binary_outputs)
                    remove_extra_outputs(owner.user_output, binary_outputs, "user_*.bin")
                    write_outputs(owner.generated_output, metadata_outputs)
                    action = "generated"
            else:
                write_outputs(owner.user_output, binary_outputs)
                remove_extra_outputs(owner.user_output, binary_outputs, "user_*.bin")
                write_outputs(owner.generated_output, metadata_outputs)
                action = "generated"
    except (FontAssetError, OwnerConfigError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    summary = {
        "assets": len(config.user_assets),
        "cache_capacity": config.user_cache_capacity,
        "size": config.user_size,
    }
    print(f"User fonts {action}: {json.dumps(summary, sort_keys=True)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

