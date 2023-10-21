#!/bin/sh
rm -rf 3daysWIN
mkdir 3daysWIN
printf '3d_loader.exe -t T' > 3daysWIN/run3days.bat
tcsh makeWIN.tcsh
cp 3d_loader.exe 3daysWIN
cp -r T 3daysWIN
a=3days-$(git rev-parse HEAD)
cp SDL2.dll HCRT.BIN 3daysWIN
zip -9 -r $a.zip ./3daysWIN
rm -rf 3daysWIN
