#!/usr/bin/env python3
"""Regression checks for the reviewed defects; native or Aztec under emu2."""
import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
parser.add_argument("--suffix", default="")
args = parser.parse_args()
directory = args.directory.resolve()
cpm = args.suffix == ".cmd"
count = 0
with tempfile.TemporaryDirectory(prefix="cmdtools-review-") as temporary:
    root = Path(temporary)
    for tool in ("bin2cmd", "cmdinfo", "cmdmod", "h862cmd", "exe2cmd",
                 "h862bin", "hex2bin"):
        shutil.copy2(directory / (tool + args.suffix), root)

    def run(tool, options, ok=True):
        global count
        command = ["./" + tool + args.suffix]
        if args.suffix:
            command.insert(0, "emu2")
        r = subprocess.run(command + options, cwd=root, stdin=subprocess.DEVNULL,
                           capture_output=True, timeout=30)
        log = r.stdout + r.stderr
        # CP/M and DOS 1 terminate without an exit status; check diagnostics.
        if not args.suffix or ok:
            assert (r.returncode == 0) == ok, (tool, options, r)
        assert (b"ERR:" not in log) == ok, (tool, options, log)
        count += 1
        return log

    def record(address, kind, data=b""):
        body = bytes([len(data), address >> 8, address & 255, kind]) + data
        return b":" + (body + bytes([-sum(body) & 255])).hex().encode() + b"\r\n"

    eof = record(0, 1)
    for tool in ("h862bin", "hex2bin"):
        assert tool.encode() in run(tool, [], False)
        (root / "plain.hex").write_bytes(record(0x100, 0, bytes(range(128))) + eof)
        run(tool, ["plain.hex", "plain.bin"])
        assert (root / "plain.bin").read_bytes() == bytes(range(128))
        (root / "zero.hex").write_bytes(record(0, 0, b"x") + record(0, 0))
        run(tool, ["zero.hex", "zero.bin"], tool == "hex2bin")
        (root / "asm.hex").write_bytes(record(0, 0x81, b"x") + eof)
        run(tool, ["asm.hex", "asm.bin"], tool == "h862bin")
        for invalid in (record(0, 0, b"x"), record(0, 0, b"x") * 2 + eof,
                        record(0, 0, b"x") + record(2, 0, b"y") + eof,
                        record(0xffff, 0, b"xy") + eof,
                        b":0100000001FF\r\n" + eof):
            (root / "bad.hex").write_bytes(invalid)
            (root / "keep.bin").write_bytes(bytes(128))
            run(tool, ["bad.hex", "keep.bin"], False)
            assert (root / "keep.bin").read_bytes() == bytes(128)
        full = b"".join(record(a, 0, bytes(range(128)))
                        for a in range(65408, -1, -128)) + eof
        (root / "all.hex").write_bytes(full)
        run(tool, ["all.hex", "all.bin"])
        assert (root / "all.bin").read_bytes() == bytes(range(128)) * 512

    payload = bytes(range(256)) * 256
    (root / "full.bin").write_bytes(payload)
    run("bin2cmd", ["full.bin", "full.bin"], False)
    assert (root / "full.bin").read_bytes() == payload
    aliases = ["FULL.BIN"] if args.suffix else ["./full.bin"]
    if not cpm:
        aliases.append("./full.bin")
    if not args.suffix:
        (root / "hard.bin").hardlink_to(root / "full.bin")
        (root / "sym.bin").symlink_to("full.bin")
        aliases += ["hard.bin", "sym.bin"]
    for alias in aliases:
        run("bin2cmd", ["full.bin", alias], False)
        assert (root / "full.bin").read_bytes() == payload
    (root / "group.$$$").write_bytes(payload)
    run("cmdmod", ["-s", "group.$$$", "group.cmd", "0"], False)
    assert (root / "group.$$$").read_bytes() == payload
    assert not (root / "group.cmd").exists()
    run("bin2cmd", ["-m", "10000000000000001", "full.bin", "bad.cmd"], False)
    assert not (root / "bad.cmd").exists()
    run("bin2cmd", ["-n", "full.bin", "full.cmd"])
    run("cmdinfo", ["-e", "full.cmd"])
    assert (root / "c0-0000.bin").read_bytes() == payload
    (root / "next.bin").write_bytes(payload[:128])
    run("cmdmod", ["-s", "next.bin", "full.cmd", "1"])
    run("cmdinfo", ["-e", "full.cmd"])
    assert (root / "d1-0000.bin").read_bytes() == payload[:128]
    for options in ([], ["-e"], ["missing.cmd"]):
        run("cmdinfo", options, False)
    (root / "short.cmd").write_bytes(b"short")
    run("cmdinfo", ["short.cmd"], False)
    # A non-first malformed descriptor must not index beyond g_type.
    h = bytearray(128)
    h[0] = 1
    h[9] = 32
    (root / "odd.cmd").write_bytes(h)
    run("cmdinfo", ["odd.cmd"])
    h[9] = 0
    h[1:3] = b"\x08\0"
    h[123:127] = struct.pack("<HH", 4, 3)
    for bit, expected in ((4, b"FLAGS(RSX,"), (5, b"P8087,"),
                          (6, b"F8087,"), (7, b",FIXUP)")):
        h[127] = 1 << bit
        (root / "flags.cmd").write_bytes(h + payload[:128] + bytes(512))
        assert expected in run("cmdinfo", ["-e", "flags.cmd"])
        assert (root / "c0-0000.bin").read_bytes() == payload[:128]
    (root / "c0-0000.bin").unlink()
    (root / "c0-0000.bin").mkdir()
    run("cmdinfo", ["-e", "flags.cmd"], False)
    (root / "c0-0000.bin").rmdir()
    (root / "UPPER.H86").write_bytes(b":0100008190EE\r\n:00000001FF\r\n")
    run("h862cmd", ["UPPER", "upper.cmd"])
    sentinel = bytes(128)
    (root / "out.$$$").write_bytes(sentinel)
    run("h862cmd", ["UPPER", "out.$$$"], False)
    assert (root / "out.$$$").read_bytes() == sentinel
    if not args.suffix:
        run("h862cmd", ["UPPER", "many.cmd"] + ["-8080"] * 64, False)
    # Complete synthetic BDOS image; verify all seven segment-word fixups.
    code = bytearray(256)
    code[6:8] = b"\x10\0"
    ds = bytearray((i * 37 + 11) & 255 for i in range(3840))
    mz = bytearray(512)
    struct.pack_into("<HHHHH", mz, 0, 0x5a4d, 0, 9, 1, 32)
    executable = bytes(mz + code + ds)
    (root / "bdos.exe").write_bytes(executable)
    for alias in (["bdos.exe"] if cpm else ["bdos.exe", "./bdos.exe"]):
        run("exe2cmd", ["bdos.exe", alias], False)
        assert (root / "bdos.exe").read_bytes() == executable
    for base in (0x60, 0x1234):
        run("exe2cmd", ["bdos.exe", "bdos.cmd", f"base={base:x}"])
        expected = bytearray(ds)
        for i in range(8):
            if i != 6:
                off = 42 + i * 4
                v = (struct.unpack_from("<H", ds, off)[0] + base) & 65535
                struct.pack_into("<H", expected, off, v)
        actual = (root / "bdos.cmd").read_bytes()
        assert actual[128:384] == code
        assert actual[384:] == expected
    for option in ("base=", "base=xyz", "base=10000", "base=60junk", "other=1",
                   "base=100000000", "base=100000001", "base=-FFFFFFFF",
                   "base=-0", "base=0x", "base=FFFFFFFFFFFFFFFFFFFFFFFF"):
        run("exe2cmd", ["bdos.exe", "bad.cmd", option], False)
    code[6:8] = b"\xff\xff"
    (root / "wrap.exe").write_bytes(mz + code + ds)
    run("exe2cmd", ["wrap.exe", "bad.cmd"], False)
    code[6:8] = b"\x10\0"
    (root / "short.exe").write_bytes(mz + code)
    run("exe2cmd", ["short.exe", "bad.cmd"], False)
    # Existing scratch/recovery data must never be reused or removed.
    for tool, options in (
        ("h862cmd", ["UPPER", "keep.cmd"]),
        ("hex2bin", ["plain.hex", "keep.cmd"]),
        ("h862bin", ["plain.hex", "keep.cmd"]),
        ("cmdmod", ["-n", "400", "keep.cmd", "0"]),
        ("cmdmod", ["-f", "1=1", "keep.cmd"]),
    ):
        before = bytes([1]) + bytes(127)
        (root / "keep.cmd").write_bytes(before)
        (root / "keep.$$$").write_bytes(b"RECOVERY")
        run(tool, options, False)
        assert (root / "keep.cmd").read_bytes() == before
        assert (root / "keep.$$$").read_bytes() == b"RECOVERY"
        (root / "keep.$$$").unlink()
print(f"PASS: {count} review regression cases ({args.suffix or 'native'})")
