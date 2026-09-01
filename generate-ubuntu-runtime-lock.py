#!/usr/bin/env python3
"""Generate the pinned Ubuntu runtime lock from local dpkg/APT metadata.

This is a maintainer tool.  End users consume the generated JSON and download
the referenced .deb files directly from the official Ubuntu archive.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import lzma
from pathlib import Path
import sys


DEFAULT_UBUNTU_ROOT = Path(
    "/data/data/com.termux/files/usr/var/lib/proot-distro/containers/ubuntu/rootfs"
)
DEFAULT_INDEX_SETS = (
    ("20250429T000000Z", Path("download-cache/ubuntu-snapshot-20250429")),
    ("20250831T000000Z", Path("download-cache/ubuntu-snapshot-20250831")),
    ("20260827T030000Z", Path("download-cache/ubuntu-snapshot-20260827")),
)

# These two SONAMEs originally came from Valve's optional runtime.  Public
# builds deliberately replace them with ordinary Ubuntu packages.
REPLACEMENTS = {
    "libSDL2-2.0.so.0": {
        "package": "libsdl2-2.0-0",
        "version": "2.30.0+dfsg-1ubuntu3.1",
        "member": "usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0.3000.0",
    },
    "libssh2.so.1": {
        "package": "libssh2-1t64",
        "version": "1.11.0-4.1build2",
        "member": "usr/lib/aarch64-linux-gnu/libssh2.so.1.0.1",
    },
}


def parse_control(text: str):
    """Yield minimally parsed Debian control paragraphs."""
    for paragraph in text.split("\n\n"):
        fields: dict[str, str] = {}
        current = None
        for line in paragraph.splitlines():
            if line[:1].isspace() and current:
                fields[current] += "\n" + line[1:]
            elif ":" in line:
                current, value = line.split(":", 1)
                fields[current] = value.lstrip()
        if fields:
            yield fields


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ubuntu-root", type=Path, default=DEFAULT_UBUNTU_ROOT)
    parser.add_argument("--manifest", type=Path, default=Path("steam-linux-libs/MANIFEST.tsv"))
    parser.add_argument("--output", type=Path, default=Path("ubuntu-runtime-lock.json"))
    parser.add_argument(
        "--index-set",
        action="append",
        metavar="SNAPSHOT=DIR",
        help="snapshot ID and directory containing its Packages(.xz) indexes",
    )
    args = parser.parse_args()

    ubuntu_root = args.ubuntu_root.resolve()
    dpkg_dir = ubuntu_root / "var/lib/dpkg"
    if not (dpkg_dir / "status").is_file():
        parser.error(f"Ubuntu dpkg/APT metadata not found below {ubuntu_root}")

    if args.index_set:
        index_sets = []
        for value in args.index_set:
            try:
                snapshot, directory = value.split("=", 1)
            except ValueError:
                parser.error(f"invalid --index-set value: {value!r}")
            index_sets.append((snapshot, Path(directory)))
    else:
        index_sets = list(DEFAULT_INDEX_SETS)

    installed = {}
    for fields in parse_control((dpkg_dir / "status").read_text(errors="replace")):
        if fields.get("Status") == "install ok installed":
            installed[fields["Package"]] = fields

    owners: dict[str, list[str]] = {}
    for listing in sorted((dpkg_dir / "info").glob("*.list")):
        package = listing.name[:-5].split(":", 1)[0]
        for path in listing.read_text(errors="replace").splitlines():
            owners.setdefault(path, []).append(package)

    archive = {}
    for snapshot, index_dir in index_sets:
        index_files = sorted(index_dir.glob("*Packages"))
        index_files += sorted(index_dir.glob("*Packages.xz"))
        if not index_files:
            parser.error(f"no arm64 Packages indexes found in {index_dir}")
        for index_file in index_files:
            if index_file.suffix == ".xz":
                index_text = lzma.decompress(index_file.read_bytes()).decode(errors="replace")
            else:
                index_text = index_file.read_text(errors="replace")
            for fields in parse_control(index_text):
                required = (
                    "Package", "Version", "Architecture", "Filename", "Size", "SHA256"
                )
                if not all(fields.get(field) for field in required):
                    continue
                key = (fields["Package"], fields["Version"], fields["Architecture"])
                previous = archive.get(key)
                metadata = {field.lower(): fields[field] for field in required}
                metadata["base_url"] = (
                    f"https://snapshot.ubuntu.com/ubuntu/{snapshot}/"
                )
                if previous:
                    # Ubuntu occasionally promotes an unchanged binary between
                    # components, giving identical bytes two valid pool paths.
                    if (previous["sha256"], previous["size"]) != (
                        metadata["sha256"], metadata["size"]
                    ):
                        raise RuntimeError(f"conflicting archive metadata for {key}")
                    continue
                archive[key] = metadata

    package_records = {}
    libraries = []
    unresolved = []
    ubuntu_prefix = str(ubuntu_root)
    for line_number, line in enumerate(args.manifest.read_text().splitlines(), 1):
        if not line.strip():
            continue
        try:
            name, old_kind, old_source = line.split("\t")
        except ValueError:
            raise RuntimeError(f"invalid manifest line {line_number}: {line!r}")

        replacement = REPLACEMENTS.get(name)
        if replacement:
            package = replacement["package"]
            version = replacement["version"]
            member = replacement["member"]
            raw_sha256 = None
        else:
            if not old_source.startswith(ubuntu_prefix + "/"):
                unresolved.append(f"{name}: unsupported source {old_source}")
                continue
            absolute_member = old_source[len(ubuntu_prefix) :]
            candidates = owners.get(absolute_member, [])
            if len(candidates) != 1:
                unresolved.append(
                    f"{name}: expected one owner for {absolute_member}, got {candidates}"
                )
                continue
            package = candidates[0]
            status = installed.get(package)
            if not status:
                unresolved.append(f"{name}: {package} is not recorded as installed")
                continue
            version = status["Version"]
            member = absolute_member.lstrip("/")
            raw_sha256 = sha256(ubuntu_root / member)

        key = (package, version, "arm64")
        metadata = archive.get(key)
        if not metadata:
            unresolved.append(f"{name}: archive has no exact {package} {version} arm64")
            continue

        package_record = {
            "name": package,
            "version": version,
            "architecture": "arm64",
            "filename": metadata["filename"],
            "size": int(metadata["size"]),
            "sha256": metadata["sha256"],
            "url": metadata["base_url"] + metadata["filename"],
            "copyright_member": f"usr/share/doc/{package}/copyright",
        }
        previous = package_records.setdefault(package, package_record)
        if previous != package_record:
            unresolved.append(f"{name}: package {package} resolves to multiple versions")
            continue

        libraries.append(
            {
                "name": name,
                "package": package,
                "member": member,
                "raw_sha256": raw_sha256,
                "origin": "ubuntu-replacement" if replacement else old_kind,
            }
        )

    if unresolved:
        print("Unable to generate a complete runtime lock:", file=sys.stderr)
        for problem in unresolved:
            print(f"  {problem}", file=sys.stderr)
        return 1

    # GCC runtime packages intentionally share their copyright directory with
    # gcc-14-base.  Include that small package as notice-only input so the
    # public fetcher never has to omit a license file or follow a dangling
    # documentation symlink.
    provider = "gcc-14-base"
    provider_status = installed.get(provider)
    if not provider_status:
        raise RuntimeError(f"copyright provider is not installed: {provider}")
    provider_key = (provider, provider_status["Version"], "arm64")
    provider_metadata = archive.get(provider_key)
    if not provider_metadata:
        raise RuntimeError(f"archive has no exact copyright provider {provider_key}")
    package_records[provider] = {
        "name": provider,
        "version": provider_status["Version"],
        "architecture": "arm64",
        "filename": provider_metadata["filename"],
        "size": int(provider_metadata["size"]),
        "sha256": provider_metadata["sha256"],
        "url": provider_metadata["base_url"] + provider_metadata["filename"],
        "copyright_member": f"usr/share/doc/{provider}/copyright",
        "notice_only": True,
    }
    for package in ("libgcc-s1", "libstdc++6"):
        package_records[package]["copyright_member"] = (
            f"usr/share/doc/{provider}/copyright"
        )

    document = {
        "schema": 1,
        "distribution": "Ubuntu 24.04 LTS (Noble), arm64",
        "snapshots": [snapshot for snapshot, _directory in index_sets],
        "packages": sorted(package_records.values(), key=lambda item: item["name"]),
        "libraries": sorted(libraries, key=lambda item: item["name"]),
    }
    args.output.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")
    print(
        f"wrote {args.output}: {len(document['libraries'])} libraries from "
        f"{len(document['packages'])} locked Ubuntu packages"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
