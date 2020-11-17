all: msdos.com mscpm86.cmd drcpm86.cmd drcpm86m.cmd drcpm86d.cmd

mscpm86.cmd:  mscpm86.com
	../bin2cmd $< $@
	../cmdinfo mscpm86.cmd

drcpm86.cmd drcpm86d.cmd drcpm86m.cmd: drcpm86.a86
	pcdev_rasm86 drcpm86.a86 
	pcdev_linkexe drcpm86
	../exe2cmd drcpm86.exe drcpm86d.cmd
	pcdev_exe2bin drcpm86.exe drcpm86.com
	pcdev_link drcpm86.obj,drcpm86.exe,,,,
	pcdev_exe2bin drcpm86.exe drcpm86.com
	../bin2cmd drcpm86.com drcpm86m.cmd
	pcdev_linkcmd drcpm86
	../cmdinfo drcpm86m.cmd
	../cmdinfo drcpm86d.cmd
	../cmdinfo drcpm86.cmd

%.com: %.exe
	pcdev_exe2bin $< $@
	@echo "";[ -f $@ ]

%.obj: %.a86
	pcdev_rasm86 $<
	@echo "";[ -f $@ ]

%.obj: %.asm
	pcdev_masm $<,$@,,nul
	@echo "";[ -f $@ ]

%.exe: %.obj
	pcdev_link $<,$@,,,,

clean:
	rm -f *.map *.lst *.obj *.exe *.com *.cmd *.crf



