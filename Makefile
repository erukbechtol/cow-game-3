ifeq ($(strip $(DEVKITPRO)),)
$(error Please set DEVKITPRO (install devkitPro))
endif
ARM     := $(DEVKITPRO)/devkitARM/bin/arm-none-eabi-
LIBNDS  := $(DEVKITPRO)/libnds
NDSTOOL := $(DEVKITPRO)/tools/bin/ndstool

cowgame3.nds: cowgame3.elf
	$(NDSTOOL) -c $@ -9 $< -7 $(LIBNDS)/default.elf

cowgame3.elf: source/main.c
	$(ARM)gcc -march=armv5te -mtune=arm946e-s -O2 -DARM9 -I$(LIBNDS)/include -specs=ds_arm9.specs $< -L$(LIBNDS)/lib -lnds9 -o $@

clean:
	rm -f cowgame3.elf cowgame3.nds
