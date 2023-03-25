CC65 = cc65
OUTDIR = ../pico/firmware_files

TARGETS = $(OUTDIR)/drv0boot.bin
# $(OUTDIR)/drv1drive.bin

HEADERS = $(wildcard *.inc)

.PHONY: all

all: $(OUTDIR) $(TARGETS)

$(OUTDIR)/drv0boot.bin: drv0boot.o
	$(CC65)\ld65 -C Cn00_RelocDriver.cfg --dbgfile $(basename $@).dbg -o $@ $<

# $(OUTDIR)/drv1drive.bin: drv1drive.o
# 	$(CC65)\ld65 -C Cn00.cfg --dbgfile $(basename $@).dbg -o $@ $<

%.o: %.s $(HEADERS)
	$(CC65)\ca65 $< -l $(basename $@).lst --debug-info

