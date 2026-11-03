#!/usr/bin/env python3
"""Raw flag edits preserve every other byte, including opaque record tails."""
from pathlib import Path
import subprocess,sys,tempfile
exe=str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix='cmdmod-flags-') as temporary:
    root=Path(temporary);p=root/'flags.cmd'
    original=bytearray((i*37+11)&255 for i in range(1031))
    original[:128]=bytes(128);original[0]=1;original[1]=16
    # Header-only edits must work even when group editing refuses these records.
    original[0x7b:0x7f]=b'\x02\0\x03\0'
    for flags in [0,255,170,85,128]:
        for bit in range(8):
            for value in range(2):
                data=bytearray(original);data[127]=flags;p.write_bytes(data)
                r=subprocess.run([exe,'-f',f'{bit}={value}',str(p)],capture_output=True)
                assert r.returncode==0,r.stderr
                data[127]=(flags&~(1<<bit))|(value<<bit)
                assert p.read_bytes()==data,(flags,bit,value)
    p.write_bytes(original)
    r=subprocess.run([exe,'-F','0=1','-f','1=1','-f','0=0',str(p)],capture_output=True)
    assert r.returncode==0,r.stderr
    expected=bytearray(original);expected[127]=2;assert p.read_bytes()==expected
    for args in [['-f','8=1'],['-f','1=2'],['-f','1'],['-f','1='],['-f','1=1x'],
                 ['-f','1=1','-d'],['-f','1=1','-t','CODE']]:
        before=p.read_bytes()
        assert subprocess.run([exe,*args,str(p)],capture_output=True).returncode!=0,args
        assert p.read_bytes()==before,args
    assert subprocess.run([exe,'-f','1=1',str(p),'0'],capture_output=True).returncode!=0
    p.write_bytes(b'MZ'+bytes(200));before=p.read_bytes()
    assert subprocess.run([exe,'-f','1=1',str(p)],capture_output=True).returncode!=0
    assert p.read_bytes()==before
    assert subprocess.run([exe,'-f','1=1',str(root/'missing.cmd')],capture_output=True).returncode!=0
    assert not(root/'missing.cmd').exists()
print('PASS: 80 raw flag edits, repeated assignments, case-insensitive switch, unchanged RSX/fixups/padding and failure paths')
