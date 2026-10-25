# BlocoGBA - compilar com devkitPro (devkitARM)
#   make          -> BlocoGBA.gba
#   make preview  -> gera prints do jogo no PC (precisa de gcc)

ifeq ($(strip $(DEVKITARM)),)
ROM_TARGETS :=
else
ROM_TARGETS := BlocoGBA.gba
endif

PREFIX  := $(DEVKITARM)/bin/arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
GBAFIX  := $(DEVKITPRO)/tools/bin/gbafix
CFLAGS  := -mthumb -mthumb-interwork -mcpu=arm7tdmi -mtune=arm7tdmi -O2 -Wall -std=gnu11

all: check $(ROM_TARGETS)

check:
ifeq ($(strip $(DEVKITARM)),)
	$(error DEVKITARM nao definido. Instale o devkitPro: https://devkitpro.org/wiki/Getting_Started)
endif

CORE := ../blococore

BlocoGBA.elf: source/main.c $(CORE)/core.c $(CORE)/art.c
	$(CC) $(CFLAGS) -I$(CORE) -specs=gba.specs source/main.c $(CORE)/core.c $(CORE)/art.c -o $@

BlocoGBA.gba: BlocoGBA.elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@ -tBLOCOGBA -cBLOC -mHR

preview:
	cd preview && gcc -std=gnu11 -O2 -DHOST_PREVIEW -I. -I../../blococore -I../source preview.c \
		../../blococore/core.c ../../blococore/art.c -o preview && ./preview

clean:
	rm -f BlocoGBA.elf BlocoGBA.gba preview/preview preview/*.ppm

.PHONY: all check preview clean
