CC=gcc
CFLAGS=$(shell pkg-config --cflags sdl2 portaudio-2.0) -Ofast -g3  -m64 -fno-omit-frame-pointer -DUSE_NETWORKING -Wno-int-conversion -fno-stack-protector
CFILES=ext/ln/linenoise.c sound.c main.c ext/dyad/src/dyad.c ext/vec/src/vec.c ext/map/src/map.c TOSPrint.c ext/C_Unescaper/escaper.c ext/argtable3/argtable3.c runtime.c poopalloc.c tos_aot.c sdl_window.c multic.c vfs.c dbg.c
AFILES=FFI_SYSV.yasm
COBJS=$(foreach o,$(CFILES),$(o).o)
AOBJS=$(foreach o,$(AFILES),$(o).o) 
OBJS=$(COBJS) $(AOBJS)
LIBS= -lm -lpthread $(shell pkg-config --libs sdl2 portaudio-2.0)
all: 3d_loader
	echo "done"
3days-0.6.00.pkg: FreeBSD_pkg.sh 3d_loader T
	./3d_loader -t T -c T/ReleasePackage.HC
	tcsh FreeBSD_pkg.sh
3days.deb: Debian_pkg.sh 3d_loader T
	./3d_loader -t T -c T/ReleasePackage.HC
	tcsh Debian_pkg.sh
3d_loader: $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o 3d_loader
%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.yasm.o: %.yasm
	yasm -f elf64 -o $@ $<
clean:
	rm $(OBJS)
