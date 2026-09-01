#!/usr/bin/env python3
"""Fetch and assemble the locked Ubuntu ARM64 userspace libraries."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request


ALLOWED_HOST = "snapshot.ubuntu.com"
RPATH = (
    "$ORIGIN:/data/data/com.termux/files/usr/glibc/lib:"
    "/data/data/com.termux/files/usr/glibc/lib/pulseaudio"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def safe_member(root: Path, member: str) -> Path:
    relative = Path(member)
    if relative.is_absolute() or ".." in relative.parts or not relative.parts:
        raise RuntimeError(f"unsafe archive member in lock: {member!r}")
    candidate = root.joinpath(relative)
    resolved_parent = candidate.parent.resolve()
    if root.resolve() not in (resolved_parent, *resolved_parent.parents):
        raise RuntimeError(f"archive member escapes extraction root: {member!r}")
    return candidate


def validate_lock(document):
    if document.get("schema") != 1:
        raise RuntimeError("unsupported Ubuntu runtime lock schema")
    packages = document.get("packages")
    libraries = document.get("libraries")
    if not isinstance(packages, list) or not isinstance(libraries, list):
        raise RuntimeError("invalid Ubuntu runtime lock")

    package_map = {}
    for package in packages:
        required = ("name", "version", "architecture", "size", "sha256", "url")
        if not all(package.get(field) for field in required):
            raise RuntimeError(f"incomplete package lock: {package!r}")
        if package["architecture"] != "arm64":
            raise RuntimeError(f"unexpected package architecture: {package!r}")
        parsed = urllib.parse.urlsplit(package["url"])
        if parsed.scheme != "https" or parsed.hostname != ALLOWED_HOST:
            raise RuntimeError(f"untrusted package URL: {package['url']}")
        if len(package["sha256"]) != 64 or any(
            char not in "0123456789abcdef" for char in package["sha256"]
        ):
            raise RuntimeError(f"invalid package hash for {package['name']}")
        if package["name"] in package_map:
            raise RuntimeError(f"duplicate package lock: {package['name']}")
        package_map[package["name"]] = package

    names = set()
    for library in libraries:
        required = ("name", "package", "member", "origin")
        if not all(library.get(field) for field in required):
            raise RuntimeError(f"incomplete library lock: {library!r}")
        if library["name"] in names or "/" in library["name"]:
            raise RuntimeError(f"invalid or duplicate library name: {library['name']}")
        if library["package"] not in package_map:
            raise RuntimeError(f"unknown package for {library['name']}")
        safe_member(Path("/lock-root"), library["member"])
        names.add(library["name"])
    return package_map


def download_locked(package, cache: Path) -> Path:
    destination = cache / f"{package['sha256']}.deb"
    expected_size = int(package["size"])
    expected_sha = package["sha256"]
    if destination.is_file() and destination.stat().st_size == expected_size:
        if sha256(destination) == expected_sha:
            return destination

    partial = destination.with_suffix(".deb.partial")
    partial.unlink(missing_ok=True)
    request = urllib.request.Request(
        package["url"], headers={"User-Agent": "steam-arm64-termux-bootstrap/1"}
    )
    try:
        with urllib.request.urlopen(request, timeout=60) as response, partial.open("xb") as out:
            shutil.copyfileobj(response, out, 1024 * 1024)
        if partial.stat().st_size != expected_size or sha256(partial) != expected_sha:
            raise RuntimeError(f"checksum or size mismatch for {package['name']}")
        partial.replace(destination)
    except BaseException:
        partial.unlink(missing_ok=True)
        raise
    return destination


def main() -> int:
    parser = argparse.ArgumentParser()
    script_root = Path(__file__).resolve().parent
    parser.add_argument("--lock", type=Path, default=script_root / "ubuntu-runtime-lock.json")
    parser.add_argument("--output", type=Path, default=script_root / "steam-linux-libs")
    parser.add_argument(
        "--notices", type=Path, default=script_root / "THIRD-PARTY-NOTICES/ubuntu"
    )
    parser.add_argument(
        "--cache",
        type=Path,
        default=script_root / "download-cache/ubuntu-debs",
    )
    args = parser.parse_args()

    document = json.loads(args.lock.read_text())
    package_map = validate_lock(document)
    args.cache.mkdir(parents=True, exist_ok=True)
    temp_parent = Path(os.environ.get("PREFIX", "/data/data/com.termux/files/usr")) / "tmp"
    temp_parent.mkdir(parents=True, exist_ok=True)

    work = Path(tempfile.mkdtemp(prefix="steam-ubuntu-runtime.", dir=temp_parent))
    extracted = work / "extracted"
    output = work / "steam-linux-libs"
    notices = work / "ubuntu-notices"
    extracted.mkdir()
    output.mkdir()
    notices.mkdir()
    try:
        archives = {}
        for number, package in enumerate(document["packages"], 1):
            print(f"[{number}/{len(document['packages'])}] {package['name']} {package['version']}")
            archive = download_locked(package, args.cache)
            archives[package["name"]] = archive
            subprocess.run(
                ["dpkg-deb", "-x", str(archive), str(extracted)],
                check=True,
                stdout=subprocess.DEVNULL,
            )

        manifest_lines = []
        for library in document["libraries"]:
            source = safe_member(extracted, library["member"])
            if not source.is_file():
                raise RuntimeError(
                    f"{library['package']} did not provide {library['member']}"
                )
            raw_expected = library.get("raw_sha256")
            if raw_expected and sha256(source) != raw_expected:
                raise RuntimeError(f"member checksum mismatch for {library['name']}")
            target = output / library["name"]
            shutil.copyfile(source, target)
            target.chmod(0o600)
            probe = subprocess.run(
                ["patchelf", "--print-needed", str(target)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            if probe.returncode == 0:
                subprocess.run(
                    ["patchelf", "--force-rpath", "--set-rpath", RPATH, str(target)],
                    check=True,
                )
            package = package_map[library["package"]]
            manifest_lines.append(
                f"{library['name']}\tubuntu-snapshot\t"
                f"{package['url']}#{library['member']}\n"
            )

        (output / "MANIFEST.tsv").write_text("".join(manifest_lines))
        official_packages = script_root / "steam-linux-official-packages.txt"
        if official_packages.is_file():
            shutil.copyfile(official_packages, output / "OFFICIAL-PACKAGES.txt")
        else:
            (output / "OFFICIAL-PACKAGES.txt").write_text("")

        for package in document["packages"]:
            copyright_path = safe_member(extracted, package["copyright_member"])
            try:
                resolved = copyright_path.resolve(strict=True)
            except FileNotFoundError as error:
                raise RuntimeError(
                    f"copyright file unavailable for {package['name']}: "
                    f"{package['copyright_member']}"
                ) from error
            if extracted.resolve() not in (resolved, *resolved.parents) or not resolved.is_file():
                raise RuntimeError(f"unsafe copyright file for {package['name']}")
            shutil.copyfile(resolved, notices / f"{package['name']}.copyright")

        for destination, staged in ((args.output, output), (args.notices, notices)):
            destination.parent.mkdir(parents=True, exist_ok=True)
            backup = destination.with_name(destination.name + ".previous")
            if backup.exists():
                shutil.rmtree(backup)
            if destination.exists():
                destination.replace(backup)
            staged.replace(destination)
        print(
            f"Ubuntu runtime ready: {len(document['libraries'])} libraries from "
            f"{len(document['packages'])} verified packages"
        )
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"fetch-ubuntu-runtime: {error}", file=sys.stderr)
        raise SystemExit(1)
