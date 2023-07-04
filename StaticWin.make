# dont use this to crosscompile, just build it like a normal person would(windows msys2)
.DEFAULT_GOAL=all

.PHONY: all clean
all: tos

CC := cc
CXX := c++
SDL2CONFIG := sdl2-config
PKGCONFIG := pkg-config
YASM := yasm
FLAGS := -O3 -Wall -Werror=shadow -fno-exceptions -fno-omit-frame-pointer           \
        -m64 -Wextra -Wno-frame-address -DTOS_STATIC_BUILD
CFLAGS := $(FLAGS) -std=gnu17 -Werror=implicit-function-declaration
CXXFLAGS := $(FLAGS) -std=gnu++17 -fno-rtti
YASMFLAGS := -f win64

C_FILES = sound.c ext/dyad.c ext/argtable3.c
CXX_FILES = main.cxx dbg.cxx mem.cxx vfs.cxx     \
           multic.cxx runtime.cxx tos_aot.cxx   \
           sdl_window.cxx TOSPrint.cxx          \
           ext/linenoise/linenoise.cpp          \
           ext/linenoise/wcwidth.cpp            \
           ext/linenoise/ConvertUTF.cpp
YASM_FILES := FFI_WIN64.yasm

LIBS := -lm -lshlwapi -ldbghelp -lsynchronization -lshcore -lssp   \
       -lws2_32 -lwinmm -d2d1 $(shell $(SDL2CONFIG) --static-libs) \
       $(shell $(PKGCONFIG) libuv-static --libs) -static-libgcc    \
       -static-libstdc++ --static

%.c.obj: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
%.cxx.obj: %.cxx
	$(CXX) $(CXXFLAGS) -c -o $@ $<
%.cpp.obj: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<
%.yasm.obj: %.yasm
	$(YASM) $(YASMFLAGS) -o $@ $<
OBJS = $(foreach o,$(C_FILES),$(o).obj)    \
       $(foreach o,$(YASM_FILES),$(o).obj) \
       $(foreach o,$(CXX_FILES),$(o).obj)
tos: $(OBJS)
	$(CXX) $(OBJS) $(LIBS) -o tos
clean:
	rm $(OBJS)
