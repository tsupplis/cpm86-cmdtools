CC=cc
CFLAGS=-O
STRIP=strip
LDFLAGS=
BINEXT=
EMU=emu2

# Runs a CMD with the emulator, checking the output of test.bin (skipped if absent)
RUNHELLO=if command -v $(EMU) >/dev/null; then $(EMU) $(1) | grep -q 'Hello from assembler'; else echo "WRN: $(EMU) not found, run skipped"; fi

TOOLS=h862bin$(BINEXT) cmdinfo$(BINEXT) cmdmod$(BINEXT) bin2cmd$(BINEXT)  exe2cmd$(BINEXT) bin2asm$(BINEXT)

all: $(TOOLS)

bin2asm$(BINEXT): bin2asm.c b2aconf.c b2aconf.h
	$(CC) $(CFLAGS) -o $@ bin2asm.c b2aconf.c $(LDFLAGS)
	$(STRIP) $@

h862bin$(BINEXT): h862bin.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

bin2cmd$(BINEXT): bin2cmd.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

cmdinfo$(BINEXT): cmdinfo.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

cmdmod$(BINEXT): cmdmod.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

exe2cmd$(BINEXT): exe2cmd.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

# Native objects are not used: cross builds (Makefile.cpm86, Makefile.dos) leave
# incompatible *.o files in this directory
clean:
	$(RM) $(TOOLS)
	$(RM) -r cmdinfo-test cmdmod-test bin2cmd-test h862bin-test

# Exercises cmdinfo (all group types, base/min/max, 8080 model warnings, extraction, errors) in ./cmdinfo-test
test-cmdinfo: cmdinfo cmdmod bin2cmd
	$(RM) -r cmdinfo-test && mkdir cmdinfo-test
	head -c 300 cmdmod.c > cmdinfo-test/a.bin
	head -c 50 cmdmod.c > cmdinfo-test/b.bin
	cd cmdinfo-test && set -e; \
	../bin2cmd ../test.bin full.cmd 2>/dev/null; \
	../cmdmod -t DATA -s b.bin -b 40 full.cmd 1 2>/dev/null; \
	../cmdmod -t EXTRA -s a.bin full.cmd 2 2>/dev/null; \
	../cmdmod -t STACK -n 400 full.cmd 3 2>/dev/null; \
	../cmdmod -t AUX1 -n 10 -m 20 full.cmd 4 2>/dev/null; \
	../cmdinfo full.cmd | tee full.txt; \
	grep -q 'INF: FILE(full.cmd)' full.txt; \
	grep -q 'BLOCK FLAGS(CMD,N8087,N8087,NFIXUP)' full.txt; \
	grep -q 'HDR(0) TYPE(01,CODE) BASE(RELOC) MIN(0.3k=304) LEN(304)' full.txt; \
	grep -q 'HDR(1) TYPE(02,DATA) BASE(FIXED,0040h) MIN(0.1k=64) LEN(64)' full.txt; \
	grep -q 'HDR(2) TYPE(03,EXTRA) BASE(RELOC) MIN(0.3k=304) LEN(304)' full.txt; \
	grep -q 'HDR(3) TYPE(04,STACK) BASE(RELOC) MIN(1.0k=1024) LEN(0)' full.txt; \
	grep -q 'HDR(4) TYPE(05,AUX #1) BASE(RELOC) MIN(0.0k=16) MAX(0.0k=32) LEN(0)' full.txt; \
	test `grep -c 'HDR(' full.txt` -eq 5; \
	if ../cmdinfo full.cmd 2>&1 | grep -E -q 'WRN|MODEL'; then exit 1; fi; \
	../cmdinfo -e full.cmd; \
	tail -c +257 c0-0000.bin | head -c 40 | cmp - ../test.bin; \
	head -c 50 d1-0040.bin | cmp - b.bin; \
	head -c 300 e2-0000.bin | cmp - a.bin; \
	test `ls c*.bin d*.bin e*.bin | wc -l` -eq 3; \
	../bin2cmd ../test.bin h.cmd 2>/dev/null; \
	../cmdinfo h.cmd 2>&1 | grep -q 'MODEL(8080)'; \
	test `../cmdinfo full.cmd h.cmd 2>/dev/null | grep -c 'INF: FILE('` -eq 2; \
	../cmdmod -t STACK -n 400 h.cmd 1 2>/dev/null; \
	../cmdmod -t EXTRA -s a.bin h.cmd 2 2>/dev/null; \
	../cmdinfo h.cmd 2>&1 | grep -q 'WRN: HDR(1) TYPE(04,STACK) is ignored by the loader in 8080 model'; \
	../cmdinfo h.cmd 2>&1 | grep -q 'WRN: HDR(2) TYPE(03,EXTRA) is ignored by the loader in 8080 model'; \
	../cmdinfo h.cmd 2>&1 | grep -q 'WRN: HDR(2) content would be loaded at segment 0'; \
	../cmdmod -t SHARED h.cmd 0 2>/dev/null; \
	../cmdinfo h.cmd 2>&1 | grep -q 'HDR(0) TYPE(09,SHARED CODE)'; \
	../cmdinfo h.cmd 2>&1 | grep -q 'WRN: no CODE group in 8080 model'; \
	../cmdmod -s b.bin s.cmd 0 2>/dev/null; \
	../cmdinfo s.cmd 2>&1 | grep -q 'WRN: HDR(0) CODE group is shorter than the 100h-byte base page'; \
	../cmdinfo nonexistent.cmd 2>&1 | grep -q 'ERR: Cannot open file'; \
	echo short > short.cmd; \
	../cmdinfo short.cmd 2>&1 | grep -q 'ERR: Cannot read header fully'; \
	if ../cmdinfo 2>/dev/null; then exit 1; fi

# Exercises cmdmod (create, resize, zero-size group, delete, test.bin round trip) in ./cmdmod-test
test-cmdmod: cmdmod cmdinfo bin2cmd
	$(RM) -r cmdmod-test && mkdir cmdmod-test
	head -c 300 cmdmod.c > cmdmod-test/a.bin
	head -c 50 cmdmod.c > cmdmod-test/b.bin
	cd cmdmod-test && set -e; \
	../cmdmod -s a.bin t.cmd 0; \
	../cmdmod -s b.bin t.cmd 1; \
	../cmdmod -t EXTRA -s a.bin t.cmd 2; \
	../cmdmod -t stack -n 400 t.cmd 3; \
	../cmdmod -s a.bin t.cmd 1; \
	../cmdmod -s b.bin t.cmd 1; \
	../cmdmod -t 9 -b 40 t.cmd 0; \
	../cmdmod -d t.cmd 0; \
	../cmdinfo t.cmd; \
	../cmdinfo -e t.cmd; \
	head -c 50 d0-0000.bin | cmp - b.bin; \
	head -c 300 e1-0000.bin | cmp - a.bin; \
	if ../cmdinfo t.cmd 2>&1 | grep -q 'MODEL(8080)'; then exit 1; fi; \
	if ../cmdmod -t foo -n 1 t.cmd 4 2>/dev/null; then exit 1; fi; \
	if ../cmdmod t.cmd 9 2>/dev/null; then exit 1; fi; \
	if ../cmdmod -d t.cmd 7 2>/dev/null; then exit 1; fi
	cd cmdmod-test && set -e; \
	../bin2cmd ../test.bin hello.cmd; \
	cp hello.cmd orig.cmd; \
	../cmdinfo -e hello.cmd; \
	../cmdmod -s c0-0000.bin hello.cmd 0; \
	cmp hello.cmd orig.cmd; \
	../cmdmod -m 1000 hello.cmd 0; \
	$(call RUNHELLO,hello.cmd); \
	cp c0-0000.bin big.bin; head -c 512 /dev/zero >> big.bin; \
	../cmdmod -s big.bin hello.cmd 0; \
	../cmdinfo hello.cmd | grep -q 'LEN(816)'; \
	$(call RUNHELLO,hello.cmd); \
	../cmdmod -n 0 -s c0-0000.bin hello.cmd 0; \
	../cmdinfo hello.cmd | grep -q 'MIN(0.3k=304).*MAX(4.0k=4096).*LEN(304)'; \
	../cmdmod -t STACK -n 400 hello.cmd 1 2>&1 | grep -q 'WRN: HDR(1) TYPE(04) is ignored'; \
	../cmdinfo hello.cmd 2>&1 | grep -q 'WRN: HDR(1) TYPE(04,STACK)'; \
	../cmdinfo hello.cmd | grep -q 'HDR(1) TYPE(04,STACK)'; \
	$(call RUNHELLO,hello.cmd); \
	../cmdmod -d hello.cmd 1; \
	test `../cmdinfo hello.cmd | grep -c HDR` -eq 1; \
	$(call RUNHELLO,hello.cmd)

# Exercises bin2cmd (zero page, -n, -m, test.bin run) in ./bin2cmd-test
test-bin2cmd: bin2cmd cmdinfo
	$(RM) -r bin2cmd-test && mkdir bin2cmd-test
	head -c 57 bin2cmd.c > bin2cmd-test/a.bin
	cd bin2cmd-test && set -e; \
	../bin2cmd a.bin a.cmd; \
	../cmdinfo a.cmd | tee a.txt; \
	grep -q 'TYPE(01,CODE).*MIN(0.3k=320).*LEN(320)' a.txt; \
	test `wc -c < a.cmd` -eq 512; \
	../bin2cmd -n a.bin n.cmd; \
	../cmdinfo n.cmd | tee n.txt; \
	grep -q 'TYPE(01,CODE).*MIN(0.1k=64).*LEN(64)' n.txt; \
	test `wc -c < n.cmd` -eq 512; \
	tail -c +129 n.cmd | head -c 57 | cmp - a.bin; \
	../bin2cmd -m 1000 a.bin m.cmd; \
	../cmdinfo m.cmd | tee m.txt; \
	grep -q 'MAX(4.0k=4096)' m.txt; \
	../cmdinfo -e a.cmd; \
	tail -c +257 c0-0000.bin | head -c 57 | cmp - a.bin; \
	../bin2cmd ../test.bin hello.cmd; \
	../cmdinfo hello.cmd | grep -q 'MIN(0.3k=304).*LEN(304)'; \
	../cmdinfo hello.cmd | grep -q 'MODEL(8080)'; \
	$(call RUNHELLO,hello.cmd); \
	../bin2cmd -m 1000 ../test.bin hello-m.cmd; \
	$(call RUNHELLO,hello-m.cmd); \
	if ../bin2cmd a.bin 2>/dev/null; then exit 1; fi

# Intel HEX and ASM-86 H86, validation failures and the full 16-bit range.
.PHONY: test-h862bin
test-h862bin: h862bin
	$(RM) -r h862bin-test && mkdir h862bin-test
	cd h862bin-test && set -e; \
	printf ':04010000DEADBEEFC3\n:00000001FF\n' > intel.hex; \
	printf '\336\255\276\357' > expected.bin; \
	../h862bin intel.hex intel.bin > stdout.txt 2>log.txt; \
	cmp intel.bin expected.bin; test ! -s stdout.txt; \
	grep -q 'INF: FILE(intel.hex) ORIGIN(0100) SIZE(4)' log.txt; \
	printf ':010103840473\n:010101820279\n:01010081017C\n:010102830376\n:0400000300000100F8\n:00000001FF\n\032garbage' > asm.hex; \
	printf '\001\002\003\004' > expected.bin; \
	../h862bin asm.hex asm.bin; cmp asm.bin expected.bin; \
	printf ':01FFFF000100\n:00000001FF\n' > last.hex; \
	../h862bin last.hex last.bin; test `wc -c < last.bin` -eq 1; \
	printf ':04010000DEADBEEFC2\n:00000001FF\n' > checksum.hex; \
	printf ':04010000DEADBEEFC3\n' > noeof.hex; \
	printf ':00000001FF\n' > empty.hex; \
	printf ':0101000001FD\n:0101020003F9\n:00000001FF\n' > hole.hex; \
	printf ':0101000001FD\n:0101000001FD\n:00000001FF\n' > overlap.hex; \
	printf ':02FFFF000102FD\n:00000001FF\n' > overflow.hex; \
	printf ':020000040000FA\n:00000001FF\n' > unsupported.hex; \
	printf ':04010000DEAD00\n:00000001FF\n' > length.hex; \
	printf ':04010000DEADXXEFC3\n:00000001FF\n' > nonhex.hex; \
	printf 'not a record\n' > invalid.hex; \
	for bad in checksum noeof empty hole overlap overflow unsupported length nonhex invalid; do \
	  printf sentinel > keep.bin; \
	  if ../h862bin $$bad.hex keep.bin > stdout.txt 2>err.txt; then exit 1; fi; \
	  grep -q '^ERR:' err.txt; test ! -s stdout.txt; \
	  printf sentinel | cmp - keep.bin; \
	done; \
	if ../h862bin intel.hex intel.hex 2>err.txt; then exit 1; fi; \
	if ../h862bin missing.hex missing.bin 2>err.txt; then exit 1; fi; \
	if ../h862bin intel.hex nowhere/out.bin 2>err.txt; then exit 1; fi; \
	if ../h862bin 2>err.txt; then exit 1; fi
	awk 'BEGIN { for (a=0; a<65536; a+=128) { printf ":80%04X00",a; s=128+int(a/256)+a%256; for (j=0;j<128;j++) { printf "%02X",j; s+=j; } printf "%02X\n",(256-s%256)%256; } print ":00000001FF"; }' > h862bin-test/full.hex
	./h862bin h862bin-test/full.hex h862bin-test/full.bin
	@test `wc -c < h862bin-test/full.bin` -eq 65536

.PHONY: all clean dist

.PHONY: test-bin2asm
test-bin2asm: bin2asm
	python3 tools/b2atest.py ./bin2asm
