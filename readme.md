# templeos in ring 3

# building
## windows users
### support only for >=Win10, msvc unsupported
install msys2, launch the "MSYS2 MINGW64 Shell", and run the following
```
pacman -Syu make yasm mingw-w64-x86_64-{gcc,SDL2,cmake,libuv}
```
## unix-like system users
install SDL2, cmake, make, yasm, portaudio and libuv
## building the loader
```
mkdir build;cd build;
cmake ..; # *nix
cmake .. -G 'MSYS Makefiles' # win32
make -j$(nproc);
```
# build runtime
```
cp HCRT_BOOTSTRAP.BIN HCRT.BIN
./tos -ctT BuiltHCRT.BIN
mv T/HCRT.BIN .
```
# run
```
./tos -t T #-h for other flags
```
# caveat
due to running in userspace, context switching is around 4 times slower <br>
division by zero is not an exception, it will bring up the debugger

# documentation
```C
Cd("T:/Server");
#include "run";
//point browser to localhost:8080
```
contributions to wiki appreciated

# ref
```C
DirMk("folder");
Dir;
Dir("fold*");//supports wildcards
Cd("folder");
Find("str",,"-i");//grep -rn . -e str
FF("file.*");//find .|grep file
Unzip("file.HC.Z");//unzip tos compression
Zip("file.HC");
DbgHelp;//help on how to debug
INT3;//force raise debug situation
ExitTOS(I32i ec=0);
```
