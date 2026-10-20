#!/usr/bin/env python3
"""Check native or emulated bin2asm against binary input, not another converter."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('program', type=Path)
parser.add_argument('--emu', action='store_true')
args = parser.parse_args()
program = args.program.resolve()
count = 0
with tempfile.TemporaryDirectory(prefix='bin2asm-') as directory:
    root = Path(directory)
    local = root / program.name
    shutil.copy2(program, local)
    command = ['emu2', './' + local.name] if args.emu else [str(local)]

    def run(options, stdin=None, ok=True):
        global count
        result = subprocess.run(command + options, cwd=root, input=stdin,
                                capture_output=True, timeout=15)
        # CP/M function 0 has no DOS-style exit status; emu2 returns zero.
        if not (args.emu and program.suffix.lower() == ".cmd" and not ok):
            assert (result.returncode == 0) == ok, (options, result)
        count += 1
        return result

    def data(text):
        values = []
        for line in text.decode().splitlines():
            line = line.split(';')[0].strip()
            if line.startswith('db '):
                for token in line[3:].split(','):
                    token = token.strip()
                    values.append(int(token[:-1], 16) if token.endswith('h')
                                  else int(token, 16))
        return bytes(values)

    payload = bytes(range(256)) * 256
    (root / 'input.bin').write_bytes(payload)
    for syntax in [[], ['-M']]:
        for size in (0, 1, 8, 9, 256, 65536):
            result = run(syntax + ['-P', '-L', f'{size:X}', '-F', 'out.asm', 'input.bin'])
            output = (root / 'out.asm').read_bytes().rstrip(b'\x1a').replace(b'\r', b'')
            assert data(output) == payload[:size], (syntax, size)
            if not args.emu:
                assert result.stdout.strip() == b'', result.stdout
            assert b'INF: FILE(' in result.stderr + result.stdout
            assert f'SIZE({size})'.encode() in result.stderr + result.stdout
            if size:
                assert b'label_0000:' in output
            # Reassemble NASM output independently to verify syntax and byte order.
            if not syntax:
                (root / 'nasm.asm').write_bytes(output)
                subprocess.run(['nasm', '-f', 'bin', 'nasm.asm', '-o', 'back.bin'],
                               cwd=root, check=True, capture_output=True)
                assert (root / 'back.bin').read_bytes() == payload[:size]
    run(['-M', '-P', '-S', '0x10', '-O', '100h', '-L', '11', 'input.bin', 'range.asm'])
    output = (root / 'range.asm').read_bytes()
    assert data(output) == payload[16:33]
    assert b'label_0110:' in output
    run(['-S', '10001', '-F', 'empty.asm', 'input.bin'])
    assert data((root / 'empty.asm').read_bytes()) == b''
    for options in [[], ['-q'], ['-s'], ['-l', '-1'], ['-o', 'xyz'],
                    ['-s', '100000000'], ['-l', '12junk'],
                    ['-f', 'out.asm', 'input.bin', 'extra.asm'],
                    ['missing.bin', 'out.asm'], ['input.bin', 'none/out.asm']]:
        if (args.emu and program.suffix.lower() == '.cmd'
                and options and options[-1] == 'none/out.asm'):
            continue
        result = run(options, ok=False)
        assert b'ERR:' in result.stderr + result.stdout, (options, result)
    before = (root / 'input.bin').read_bytes()
    run(['input.bin', 'input.bin'], ok=False)
    assert (root / 'input.bin').read_bytes() == before
    result = run(['-H'])
    assert b'INF: bin2asm' in result.stderr + result.stdout
    if not args.emu:
        result = run(['-m', '-s', '1', '-l', '9', '-'], stdin=payload[:16])
        assert data(result.stdout) == payload[1:10]
        result = run(['--', 'input.bin'])
        assert data(result.stdout) == payload
print(f'PASS: {count} bin2asm cases ({program.name}); ranges, errors, labels and NASM round trips')
