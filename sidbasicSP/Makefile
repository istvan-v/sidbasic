
CC = gcc
CXX = g++
CCFLAGS = -Wall -O2
AY_DA_OPT = ./ay_da_opt
YM_DA_OPT = ./ym_da_opt
C64CONV = ./c64conv
TAPEENC = ./tapeenc
TZXFILE = sidbasicSP.tzx
TAPFILE = sidbasicSP.tap
SJASM = sjasm
EPCOMPRESS = ../../ep128emu/epcompress

all: $(TZXFILE) $(TAPFILE)

$(TZXFILE): $(TAPEENC) loader.out sidbintSP_tzx.out
	$(TAPEENC) $@ SIDBASIC loader.out 0x4253 sidbintSP_tzx.out

$(TAPFILE): $(TAPEENC) taploader.out sidbintSP_tap.out
	$(TAPEENC) -tap $@ SIDBASIC taploader.out sidbintSP_tap.out

loader.out taploader.out: %.out: %.s
	$(SJASM) $< $@

sidbintSP_tzx.out: sidbintSP_tzx.s sidsynth.bin cod47scr.bin
	$(SJASM) $< $@

sidbintSP_tap.out: sidbintSP_tap.s sidsynth.bin cod47scr.bin
	$(SJASM) $< $@

sidsynth.bin: sidsynth.s dactable.s dactableYM.s
	$(SJASM) $< $@
	$(EPCOMPRESS) -raw -m2 -6 $@ $@

cod47scr.bin: cod47scr.s sidc64.scr
	$(SJASM) $< $@
	$(EPCOMPRESS) -raw -m2 -6 $@ $@

sidc64.scr: sidm2.scr $(C64CONV)
	$(EPCOMPRESS) -raw -m2 -x $< $@
	$(C64CONV) $@ $@

dactable.s: $(AY_DA_OPT)
	$(AY_DA_OPT) 249

dactableYM.s: $(YM_DA_OPT)
	$(YM_DA_OPT) 247

$(AY_DA_OPT): ay_da_opt.cpp
	$(CXX) $(CCFLAGS) -DUSE_DACTABLE_AY $< -o $@ -lm -s

$(YM_DA_OPT): ay_da_opt.cpp
	$(CXX) $(CCFLAGS) -DUSE_DACTABLE_YM $< -o $@ -lm -s

$(C64CONV): c64conv.c
	$(CC) $(CCFLAGS) $< -o $@ -s

$(TAPEENC): tapeenc.cpp
	$(CXX) $(CCFLAGS) $< -o $@ -s

$(EPCOMPRESS):
	cd ../../ep128emu && scons -j 4 epcompress

sidbintSP_tzx.s: sidbintSP.s
	sed -e 's/USE_ROM_LOADER[[:space:]]\+equ[[:space:]]\+[01]/USE_ROM_LOADER equ 0/' < $< > $@

sidbintSP_tap.s: sidbintSP.s
	sed -e 's/USE_ROM_LOADER[[:space:]]\+equ[[:space:]]\+[01]/USE_ROM_LOADER equ 1/' < $< > $@

clean:
	-rm $(AY_DA_OPT) $(YM_DA_OPT) $(C64CONV) $(TAPEENC) *.lst *.out *.bin *.raw *.txt

distclean: clean
	-rm $(TZXFILE) $(TAPFILE) dactable.s dactableYM.s sidc64.scr
	-rm sidbintSP_tzx.s sidbintSP_tap.s

