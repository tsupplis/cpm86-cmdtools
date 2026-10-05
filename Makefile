CC=cc
CFLAGS=-O -c
STRIP=strip
LDFLAGS=
LD=cc
BINEXT=
EMU=emu2

# Runs a CMD with the emulator, checking the output of test.bin (skipped if absent)
RUNHELLO=if command -v $(EMU) >/dev/null; then $(EMU) $(1) | grep -q 'Hello from assembler'; else echo "WRN: $(EMU) not found, run skipped"; fi

TOOLS=cmdinfo$(BINEXT) cmdmod$(BINEXT) bin2cmd$(BINEXT)  exe2cmd$(BINEXT) 

all: $(TOOLS)

bin2cmd$(BINEXT): bin2cmd.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

cmdinfo$(BINEXT): cmdinfo.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

cmdmod$(BINEXT): cmdmod.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

exe2cmd$(BINEXT): exe2cmd.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

.c.o:
	$(CC) $(CFLAGS) $<

clean:
	$(RM) *.o $(TOOLS)
	$(RM) -r cmdmod-test bin2cmd-test

test-cmdinfo: $(TOOLS)
	./cmdinfo t0.cmd
	./cmdinfo t1.cmd
	./cmdinfo t2.cmd
	./cmdinfo t3.cmd

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
