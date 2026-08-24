import argparse
import hashlib
from pathlib import Path

from patch_force_quit_bundle import parse_bundle, write_bundle


# AssetsManagerStart.CheckUpdateTipUI, current resver=795 build.
#
# Original:
#   ldarg.0
#   ldfld bool AssetsManagerStart::isTip
#   brtrue.s SET_UPDATE_STATE
#   ldarg.0
#   ldfld bool AssetsManagerStart::isForced
#   brfalse.s CALLBACK
#
# Patched:
#   ldarg.0
#   ldfld bool AssetsManagerStart::isTip
#   pop
#   nop
#   ldarg.0
#   ldfld bool AssetsManagerStart::isForced
#   brfalse.s CALLBACK
#
# This keeps the forced-update path (updatetype=2) and lets the optional-tip
# path (updatetype=1) continue through the existing callback.
OLD_CONTEXT = bytes.fromhex(
    "28 54 00 00 0A 02 7B 72 00 00 04 2D 08 "
    "02 7B 71 00 00 04 2C 1C 28 83 01 00 06"
)
NEW_CONTEXT = bytes.fromhex(
    "28 54 00 00 0A 02 7B 72 00 00 04 26 00 "
    "02 7B 71 00 00 04 2C 1C 28 83 01 00 06"
)
# Unity 2022.3.62f3 / current preternatural cache build.
CURRENT_OLD_CONTEXT = bytes.fromhex(
    "02 7B 72 00 00 04 2D 08 02 7B 71 00 00 04 2C 1C"
)
CURRENT_NEW_CONTEXT = bytes.fromhex(
    "02 7B 72 00 00 04 26 00 02 7B 71 00 00 04 2C 1C"
)


def patch_bundle(source: Path, destination: Path) -> None:
    source_bytes = source.read_bytes()
    bundle = parse_bundle(source)
    matches: list[tuple[str, int, bytes, bytes]] = []
    for node in bundle.nodes:
        node_data = bytes(bundle.data[node.offset : node.offset + node.size])
        for old_context, new_context in ((OLD_CONTEXT, NEW_CONTEXT), (CURRENT_OLD_CONTEXT, CURRENT_NEW_CONTEXT)):
            cursor = 0
            while True:
                cursor = node_data.find(old_context, cursor)
                if cursor < 0:
                    break
                matches.append((node.path, node.offset + cursor, old_context, new_context))
                cursor += 1

    if len(matches) != 1 or not matches[0][0].startswith("CAB-"):
        raise ValueError(f"expected one current-build anchor, got {[(p, hex(o)) for p, o, _, _ in matches]}")

    node_path, absolute_offset, old_context, new_context = matches[0]
    bundle.data[absolute_offset : absolute_offset + len(old_context)] = new_context
    write_bundle(bundle, destination)

    rebuilt = parse_bundle(destination)
    rebuilt_data = bytes(rebuilt.data)
    if OLD_CONTEXT in rebuilt_data or CURRENT_OLD_CONTEXT in rebuilt_data:
        raise ValueError("rebuilt bundle verification failed")
    if rebuilt_data.count(NEW_CONTEXT) + rebuilt_data.count(CURRENT_NEW_CONTEXT) != 1:
        raise ValueError("rebuilt bundle verification failed")

    print(f"source_sha256={hashlib.sha256(source_bytes).hexdigest().upper()}")
    print(
        "destination_sha256="
        f"{hashlib.sha256(destination.read_bytes()).hexdigest().upper()}"
    )
    print(f"patched={node_path} decompressed_offset=0x{absolute_offset:X}")
    print("old=2D-08 new=26-00 role=optional-update-tip")
    print(f"output={destination} size={destination.stat().st_size}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    patch_bundle(args.source, args.destination)


if __name__ == "__main__":
    main()
