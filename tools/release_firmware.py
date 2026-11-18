#!/usr/bin/env python3
# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Local release helper: build, sign, checksum, and prepare a GitHub Release.

Git tag != GitHub Release != signed OTA package. This tool coordinates them
deliberately but never publishes anything silently:

  * builds the network firmware for both targets with PlatformIO,
  * signs each firmware.bin with the PRIVATE key (kept outside the repo),
  * writes SHA256SUMS.txt and release notes into dist/<version>/,
  * verifies every package against the key's public half,
  * with --create-draft, creates a DRAFT GitHub Release via `gh` (if
    installed) attaching only the .bkota files, SHA256SUMS.txt and notes.

The private key is never uploaded, copied, or printed.
"""

import argparse
import ast
import hashlib
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sign_firmware as sf  # noqa: E402
import release_notes as rn

ROOT = sf.REPO_ROOT
ENVS = {"brooklyn-s3": "brooklyn-s3-network", "xiao-s3": "xiao-s3-network"}
PLATFORMIO = Path.home() / ".platformio/penv/Scripts/platformio.exe"


def check_developer_credentials(payload):
    """Refuse release binaries containing local development string credentials."""
    secrets = ROOT / "include" / "secrets.h"
    if not secrets.exists():
        return
    # Accept definitions with or without trailing whitespace/comments.
    definitions = re.findall(
        r'^\s*#define\s+(\w+)\s+("(?:\\.|[^"\\])*")',
        secrets.read_text(encoding="utf-8"), re.MULTILINE,
    )
    for name, literal in definitions:
        if not any(label in name.upper() for label in
                   ("PASSWORD", "SSID", "TOKEN", "SECRET")):
            continue
        value = ast.literal_eval(literal).encode("utf-8")
        if value and value in payload:
            sys.exit("refusing release: embedded developer credential " + name)


def run(cmd, **kwargs):
    print("+", " ".join(str(c) for c in cmd))
    return subprocess.run([str(c) for c in cmd], cwd=ROOT, **kwargs)


def git_output(*args):
    return subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--version", required=True, help="e.g. 4.23.0 (digits and dots recommended)")
    p.add_argument("--private-key", required=True, help="PEM outside the repository")
    p.add_argument("--key-id", type=int, default=1)
    p.add_argument("--tag", help="existing git tag for this release (default: v<version>)")
    p.add_argument("--skip-build", action="store_true", help="reuse existing .pio builds")
    p.add_argument("--allow-dirty", action="store_true")
    p.add_argument("--summary-file", help="UTF-8 Markdown file with concise inline release notes for the GitHub page")
    p.add_argument("--create-draft", action="store_true",
                   help="create a DRAFT GitHub Release with gh (never auto-published)")
    args = p.parse_args(argv)

    if re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", args.version) is None:
        sys.exit("version must be dotted numeric, for example 4.23.0")

    key = sf.load_private_key(args.private_key)  # refuses keys inside the repo
    tag = args.tag or "v" + args.version
    if tag != "v" + args.version:
        sys.exit("automatic OTA requires the release tag v<version>")
    if git_output("status", "--porcelain") and not args.allow_dirty:
        sys.exit("working tree is dirty; commit first or pass --allow-dirty")
    tag_commit = git_output("rev-list", "-n", "1", tag)
    head = git_output("rev-parse", "HEAD")
    if not tag_commit:
        print("NOTE: tag %s does not exist yet. Create and push it deliberately "
              "before publishing the release." % tag)
    elif tag_commit != head:
        sys.exit("tag %s does not point at HEAD" % tag)

    out = ROOT / "dist" / args.version
    out.mkdir(parents=True, exist_ok=True)

    assets = []
    for target, env in ENVS.items():
        build_dir = ROOT / ".pio" / "build" / env
        if not args.skip_build:
            os.environ["BROOKLYN_FIRMWARE_VERSION"] = args.version
            if run([PLATFORMIO, "run", "-e", env]).returncode != 0:
                sys.exit("build failed for %s" % env)
        version_file = build_dir / "brooklyn_firmware_version.txt"
        if not version_file.exists() or version_file.read_text(
            encoding="ascii"
        ).strip() != args.version:
            sys.exit(
                "build for %s is not version %s; rebuild without --skip-build"
                % (env, args.version)
            )
        firmware = build_dir / "firmware.bin"
        check_developer_credentials(firmware.read_bytes())
        package = out / ("%s-network-%s.bkota" % (target, args.version))
        data = sf.build_package(firmware.read_bytes(), target, args.version,
                                args.key_id, key)
        package.write_bytes(data)
        sf.verify_package(package.read_bytes(), key.public_key())
        assets.append(package)
        print("signed %s (%d bytes)" % (package.name, len(data)))

    sums = out / "SHA256SUMS.txt"
    sums.write_text(
        "".join("%s *%s\n" % (hashlib.sha256(a.read_bytes()).hexdigest(), a.name)
                for a in assets),
        encoding="ascii",
    )
    notes = out / "RELEASE_NOTES.md"
    source_notes=ROOT / "docs" / ("RELEASE_"+args.version+".md")
    if source_notes.exists():
        notes.write_text(rn.read_notes(source_notes)+"\nSource commit: "+head+"\n",encoding="utf-8")
    elif not notes.exists():
        sys.exit("Write docs/RELEASE_%s.md before preparing a release" % args.version)
    print("Prepared:", *[a.name for a in assets], sums.name, notes.name, sep="\n  ")

    files = [*assets, sums, notes]
    if args.create_draft:
        if not shutil.which("gh"):
            print("gh is not installed; upload manually (see docs/PHASE4W_SECURE_WEB_OTA.md).")
            return 0
        if not tag_commit:
            sys.exit("refusing to create a release for a tag that does not exist")
        if not args.summary_file:
            sys.exit("--create-draft requires --summary-file with readable inline notes; full notes remain attached")
        summary=Path(args.summary_file).read_text(encoding="utf-8-sig")
        page=out/"RELEASE_PAGE.md"
        page.write_text(rn.compact_body(args.version,summary),encoding="utf-8")
        cmd = ["gh", "release", "create", tag, "--draft", "--verify-tag",
               "--title", "Brooklyn Map %s" % args.version,
               "--notes-file", page, *files]
        result=run(cmd)
        if result.returncode:return result.returncode
        metadata=subprocess.run(["gh","api","repos/jgoldwein/BrooklynSubway/releases/tags/"+tag],capture_output=True,text=True,check=True)
        import json
        release=json.loads(metadata.stdout)
        if rn.metadata_bytes(release,release.get("body") or "")>rn.METADATA_LIMIT:
            sys.exit("Draft remains unpublished: release metadata exceeds the legacy OTA budget; shorten the inline summary")
        return 0
    print("Review dist/%s, then re-run with --create-draft or upload manually." % args.version)
    return 0


if __name__ == "__main__":
    sys.exit(main())
