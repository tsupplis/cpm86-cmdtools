CC=cc
CFLAGS=-O -c
STRIP=strip
LDFLAGS=
LD=cc
BINEXT=

TOOLS=cmdinfo$(BINEXT) bin2cmd$(BINEXT) 

all: $(TOOLS)

bin2cmd$(BINEXT): bin2cmd.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

cmdinfo$(BINEXT): cmdinfo.o
	$(LD) -o $@ $< $(LDFLAGS)
	$(STRIP) $@

.c.o:
	$(CC) $(CFLAGS) $<

clean:
	$(RM) *.o $(TOOLS)

test: cmdinfo
	./cmdinfo eraq.cmd
	./cmdinfo pcever.cmd
	./cmdinfo ted.cmd
	./cmdinfo cpm.sys
