CC=cc
CFLAGS=-O -c
STRIP=strip
LDFLAGS=
LD=cc
BINEXT=

TOOLS=cmdinfo$(BINEXT) bin2cmd$(BINEXT)  exe2cmd$(BINEXT) 

all: $(TOOLS)

bin2cmd$(BINEXT): bin2cmd.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

cmdinfo$(BINEXT): cmdinfo.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

exe2cmd$(BINEXT): exe2cmd.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

.c.o:
	$(CC) $(CFLAGS) $<

clean:
	$(RM) *.o $(TOOLS)

test: $(TOOLS)
	./cmdinfo t0.cmd
	./cmdinfo t1.cmd
	./cmdinfo t2.cmd
	./cmdinfo t3.cmd
