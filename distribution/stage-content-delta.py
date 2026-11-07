"""Stage a pinned Rapid game against the previously verified content formula."""

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import shutil


RAPID_INDEX = Path("rapid/repos-cdn.beyondallreason.dev/byar/versions.gz")


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read_pool_records(package):
    data = gzip.decompress(package.read_bytes())
    cursor = 0
    records = {}
    while cursor < len(data):
        name_length = data[cursor]
        cursor += 1 + name_length
        if not name_length or cursor + 24 > len(data):
            raise ValueError("Truncated or invalid Rapid SDP record")
        digest = data[cursor : cursor + 16].hex()
        length = int.from_bytes(data[cursor + 20 : cursor + 24], "big")
        if digest in records and records[digest] != length:
            raise ValueError(f"Inconsistent lengths for pool object {digest}")
        records[digest] = length
        cursor += 24
    if not records:
        raise ValueError("Rapid package contains no pool objects")
    return records


def copy_pinned(source, relative, assets, pins):
    target = assets / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    checksum = sha256(source)
    if sha256(target) != checksum:
        raise ValueError(f"Copied content differs: {relative}")
    pins[str(relative)] = checksum


def rapid_line(index, identity, version):
    rows = gzip.decompress(index.read_bytes()).decode().splitlines()
    matches = [
        row for row in rows
        if (fields := row.split(","))[1:2] == [identity] and fields[-1] == version
    ]
    if len(matches) != 1:
        raise ValueError("Expected one exact Rapid game identity")
    return matches[0]


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--base", type=Path, required=True)
parser.add_argument("--content", type=Path, required=True)
parser.add_argument("--package-id", required=True)
parser.add_argument("--package-sha256", required=True)
parser.add_argument("--game-version", required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
if not re.fullmatch(r"[0-9a-f]{32}", args.package_id):
    parser.error("Package ID must be a lowercase Rapid MD5")
if not re.fullmatch(r"[0-9a-f]{64}", args.package_sha256):
    parser.error("Package pin must be a lowercase SHA-256")
relative_package = Path("packages") / f"{args.package_id}.sdp"
package = args.content / relative_package
if sha256(package) != args.package_sha256:
    raise ValueError("Content SDP does not match the producer pin")
if (args.base / relative_package).exists():
    raise ValueError("Expected a new Rapid identity, separate from the base formula")
identity_line = rapid_line(args.content / RAPID_INDEX, args.package_id, args.game_version)
records = read_pool_records(package)

args.output.mkdir()
assets = args.output / "assets"
pins = {}
copy_pinned(package, relative_package, assets, pins)
reused = 0
for digest, length in sorted(records.items()):
    relative = Path("pool") / digest[:2] / f"{digest[2:]}.gz"
    if (args.base / relative).is_file():
        reused += 1
        continue
    source = args.content / relative
    contents = gzip.decompress(source.read_bytes())
    if len(contents) != length or hashlib.md5(contents).hexdigest() != digest:
        raise ValueError(f"New pool object does not match its SDP entry: {relative}")
    copy_pinned(source, relative, assets, pins)

# Install this index only inside the bundle's immutable Rapid lookup. The SDP
# and its pool objects must be adjacent in the user's writable content root.
(args.output / "versions.gz").write_bytes(gzip.compress((identity_line + "\n").encode(), mtime=0))
(args.output / "SHA256SUMS").write_text("".join(f"{checksum}  {path}\n" for path, checksum in sorted(pins.items())))
(args.output / "CONTENT-DELTA.json").write_text(json.dumps({
    "status": "STAGED_DELTA_REQUIRES_EXACT_APP_INSTALLATION_TEST",
    "gameVersion": args.game_version,
    "gamePackageId": args.package_id,
    "sdpSHA256": args.package_sha256,
    "rapidIndexSHA256": sha256(args.output / "versions.gz"),
    "uniquePoolObjects": len(records),
    "poolObjectsReusedFromPinnedBase": reused,
    "newPoolObjectsVerifiedAgainstSDP": len(records) - reused,
    "deltaAssets": pins,
}, indent=2) + "\n")
