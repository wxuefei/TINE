#/bin/tcsh
set STAGEDIR = `pwd`/debian
mkdir -p ${STAGEDIR}
if -e ${STAGEDIR}/usr then
    rm -r ${STAGEDIR}/usr
endif
mkdir -p ${STAGEDIR}/usr
mkdir -p ${STAGEDIR}/usr/local
mkdir -p ${STAGEDIR}/usr/local/bin
mkdir -p ${STAGEDIR}/usr/local/include/3Days
cp 3d_loader ${STAGEDIR}/usr/local/bin/3d
strip ${STAGEDIR}/usr/local/bin/3d
chmod +x ${STAGEDIR}/usr/local/bin/3d
rm ${STAGEDIR}/plist
echo "/usr/local/bin/3d" >> ${STAGEDIR}/plist
cp HCRT.BIN ${STAGEDIR}/usr/local/include/3Days/
cp HCRT_BC.BIN ${STAGEDIR}/usr/local/include/3Days/
echo "/usr/local/include/3Days/HCRT.BIN" >> ${STAGEDIR}/plist
./3d_loader -t T -c T/ReleasePackage.HC
cd T/T
set files = ( ` find . ` )
set prefix = ${STAGEDIR}/usr/local/include/3Days/T/
foreach f ($files)
    if -d $f then
        mkdir -p $prefix/$f
    else
        cp  $f  "$prefix/$f"
	echo "$prefix/$f" >> ${STAGEDIR}/plist
    endif
end
cd -
rm -r T/T
dpkg-deb --build debian 3days.deb
