# BlocoDS - Makefile para BlocksDS (https://blocksds.skylyrac.net)
# Compilar:  make         -> BlocoDS.nds
# Prints:    make preview -> gera as imagens das duas telas no PC (precisa de gcc e python3-pil)

BLOCKSDS     ?= /opt/blocksds/core

NAME         := BlocoDS
GAME_TITLE   := BlocoDS
GAME_SUBTITLE:= Clone de blocos
GAME_AUTHOR  := hroderik

SOURCEDIRS   := source ../blococore
INCLUDEDIRS  := ../blococore
DEFINES      :=
LIBS         := -lnds9
LIBDIRS      := $(BLOCKSDS)/libs/libnds

-include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile

preview:
	cd preview && gcc -std=gnu11 -O2 -DHOST_PREVIEW -I. -I../../blococore preview.c hostgl.c \
		../../blococore/core.c ../../blococore/art.c -lm -o preview && rm -f blocods.sav && ./preview && python3 compose.py

.PHONY: preview
