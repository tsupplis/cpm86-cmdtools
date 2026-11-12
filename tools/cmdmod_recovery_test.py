#!/usr/bin/env python3
"""Inject failures into every shared non-atomic output transaction."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WRAPPER = r'''
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
static int calls, backup_unlinks;
int test_rename(const char *a, const char *b) {
    int fail = atoi(getenv("FAIL_STAGE"));
    calls++;
    if (calls == fail || (fail == 4 && calls >= 2)) {
        errno = EACCES; return -1;
    }
    /* Model CP/M's inability to replace a destination by rename. */
    if (access(b, F_OK) == 0) { errno = EEXIST; return -1; }
    return rename(a, b);
}
int test_unlink(const char *p) {
    if (strstr(p, ".BAK") && ++backup_unlinks == 2 && atoi(getenv("FAIL_STAGE")) == 5) {
        errno = EACCES; return -1;
    }
    return unlink(p);
}
'''
with tempfile.TemporaryDirectory(prefix="output-recovery-") as folder:
    root = Path(folder)
    original = bytes([1]) + bytes(127) + b"original payload"
    cases = (
        ("flags", "cmdmod.c", ["-f", "1=1", "file.cmd"], []),
        ("group", "cmdmod.c", ["-n", "400", "file.cmd", "0"], []),
        ("h86", "h862cmd.c", ["input.h86", "file.cmd", "-8080"], []),
        ("hex", "hexbin.c", ["input.hex", "file.cmd"], [str(ROOT / "hex2bin.c")]),
    )
    for label, source, options, extra in cases:
        build = root / label; build.mkdir()
        (build / "test.c").write_text(WRAPPER)
        binary = build / "test"
        cc = os.environ.get("CC_HOST", "cc")
        # Redirect only the module's I/O; mocks call the real libc functions.
        subprocess.run([cc, "-DFILE_REPLACE_NONATOMIC",
                        "-Drename=test_rename", "-Dunlink=test_unlink",
                        "-I" + str(ROOT), "-c", str(ROOT / "filerepl.c"),
                        "-o", str(build / "filerepl.o")], check=True)
        subprocess.run([cc, "-I" + str(ROOT), str(ROOT / source),
                        str(build / "test.c"), str(build / "filerepl.o"), *extra,
                        "-o", str(binary)], check=True)
        updated = None
        for stage in (0, 1, 2, 4, 5):
            work = build / str(stage); work.mkdir()
            (work / "file.cmd").write_bytes(original)
            (work / "input.h86").write_bytes((ROOT / "sample.h86").read_bytes())
            (work / "input.hex").write_bytes(b":0100000041BE\n:00000001FF\n")
            result = subprocess.run([str(binary), *options], cwd=work,
                                    env=dict(os.environ, FAIL_STAGE=str(stage)), capture_output=True)
            assert (result.returncode == 0) == (stage == 0), (label, stage, result.stderr)
            if stage == 0: updated = (work / "file.cmd").read_bytes()
            expected = {
                0: {"file.cmd": updated},
                1: {"file.cmd": original, "file.$$$": updated},
                2: {"file.cmd": original, "file.$$$": updated},
                4: {"file.BAK": original, "file.$$$": updated},
                5: {"file.cmd": updated, "file.BAK": original},
            }[stage]
            actual = {p.name: p.read_bytes() for p in work.glob("file.*")}
            assert actual == expected, (label, stage, actual)
        for existing in ("file.BAK", "file.$$$"):
            work = build / ("collision" + existing[-3:]); work.mkdir()
            (work / "file.cmd").write_bytes(original)
            (work / existing).write_bytes(b"previous recovery data")
            (work / "input.h86").write_bytes((ROOT / "sample.h86").read_bytes())
            (work / "input.hex").write_bytes(b":0100000041BE\n:00000001FF\n")
            result = subprocess.run([str(binary), *options], cwd=work,
                                    env=dict(os.environ, FAIL_STAGE="0"), capture_output=True)
            assert result.returncode != 0, label
            assert (work / "file.cmd").read_bytes() == original
            assert (work / existing).read_bytes() == b"previous recovery data"
        print("PASS:", label, "replacement, preserve/install/restore/cleanup failures and recovery collisions")
