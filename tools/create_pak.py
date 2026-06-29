#!/usr/bin/env python3

import os
import struct
import sys


def write_u32(f, value):
    f.write(struct.pack("<I", value))


def collect_files(input_dir):
    files = []

    for root, _, filenames in os.walk(input_dir):
        for filename in filenames:
            full_path = os.path.join(root, filename)

            rel_path = os.path.relpath(full_path, input_dir)
            rel_path = rel_path.replace("\\", "/")

            size = os.path.getsize(full_path)

            files.append({
                "name": rel_path,
                "path": full_path,
                "size": size,
            })

    files.sort(key=lambda x: x["name"])
    return files


def create_pak(input_dir, output_pak):
    files = collect_files(input_dir)

    # Calculate offsets relative to start of data section
    current_offset = 0

    for entry in files:
        entry["offset"] = current_offset
        current_offset += entry["size"]

    with open(output_pak, "wb") as f:
        # Entry count
        write_u32(f, len(files))

        # Directory
        for entry in files:
            name_bytes = entry["name"].encode("utf-8")

            write_u32(f, len(name_bytes) + 1)
            f.write(name_bytes)

            write_u32(f, entry["offset"])
            write_u32(f, entry["size"])

        # Data
        for entry in files:
            print(f"Packing {entry['name']}")

            with open(entry["path"], "rb") as infile:
                data = infile.read()

            # Apply bitwise NOT
            data = bytes((~b) & 0xFF for b in data)

            f.write(data)

    print(f"Created {output_pak}")
    print(f"Files packed: {len(files)}")


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input_dir> <output.pak>")
        sys.exit(1)

    create_pak(sys.argv[1], sys.argv[2])


if __name__ == "__main__":
    main()