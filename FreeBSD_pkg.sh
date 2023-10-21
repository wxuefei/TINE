#/bin/tcsh
set STAGEDIR = `pwd`/stagedir
if ! -e $STAGEDIR then
	mkdir stagedir
endif
cp "+MANIFEST" ${STAGEDIR}/
mkdir -p ${STAGEDIR}
mkdir -p ${STAGEDIR}/usr
mkdir -p ${STAGEDIR}/usr/local
mkdir -p ${STAGEDIR}/usr/local/bin
mkdir -p ${STAGEDIR}/usr/local/include/3Days
cp HCRT.BIN ${STAGEDIR}/usr/local/include/3Days
cp 3d_loader ${STAGEDIR}/usr/local/bin/3d
strip ${STAGEDIR}/usr/local/bin/3d
chmod +x ${STAGEDIR}/usr/local/bin/3d
rm ${STAGEDIR}/plist
echo "/usr/local/bin/3d" >> ${STAGEDIR}/plist
echo "/usr/local/include/3Days/HCRT.BIN" >> ${STAGEDIR}/plist
./3d_loader -t T -c T/ReleasePackage.HC
echo "/usr/local/include/3Days/HCRT_BC.BIN" >> ${STAGEDIR}/plist
cp HCRT_BC.BIN ${STAGEDIR}/usr/local/include/3Days
cd T/T
set files = ( ` find . ` )
foreach f ($files)
        if ! -d $f then
    		cp  $f  ${STAGEDIR}/usr/local/include/3Days/T/$f
    		echo "/usr/local/include/3Days/T/$f" >> ${STAGEDIR}/plist
	else
		mkdir -p  ${STAGEDIR}/usr/local/include/3Days/T/$f 
	endif 
end
cd -
pkg create -m ${STAGEDIR}/ -r ${STAGEDIR}/ -p ${STAGEDIR}/plist -o .
