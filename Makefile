
TARGET = EBOOT
OBJS = src/main.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)

LIBS = -lpng -lz -lpspgu -lpspdisplay -lpspctrl -lpspkernel

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Crinmison

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
