#/bin/tcsh
set BungisFile = "BungisFile"
set BungisTmp = "BungisTmp"
set to_build = ( )
set no_clear = 0
if ($#argv) then
  foreach arg ( $argv) 
    if ( $arg == "--noclear" ) then
      set no_clear = 1
    else 
      set to_build = ( $to_build $arg )
    endif
  end
else
   set to_build = ( ` tail -n1  $BungisFile | cut -d"=" -f1 ` )
endif
if ( ! -e $BungisTmp ) then
  if ( ! $no_clear ) rm $BungisTmp
  touch $BungisTmp
else
  set found = 0
  foreach tb ( $to_build ) 
     grep "$tb" $BungisTmp >& /dev/null
     @ found += ! $?
  end
  if ( $#to_build == $found ) goto fin
endif

set lines = ` wc -l $BungisFile | tr -s ' ' ' ' | cut -d' ' -f2 `
set built = ( )
loop_again:
set line = 1
set build_cnt = 0
foreach tb0 ( $to_build )
  set tmp = ( ` mktemp ` )
  set tmp2 = ( ` mktemp ` )
  set tmp3= ( `mktemp` ) 
  echo "$tb0" > $tmp
  grep "$tb0" $BungisFile   | sort -t"=" -d > $tmp2
  join -t"=" $tmp $tmp2 > $tmp3
  set result = `cat $tmp3 | cut -d'=' -f1 `
  set command  = ` cat $tmp3 | cut -d'=' -f2   `
  set deps  = ( ` cat $tmp3 | cut -d'=' -f3  | tr ',' ' ' ` )
  set found = 0
  if($#deps) then
    if( $deps[1] == "AllAbove" ) then
      set txt = ( `cat $tmp ` )
      set line  = ` grep -n "$txt" $BungisFile | tr ':' ' ' | cut -d' ' -f1 ` 
      @ lmo = $line - 1 
      set deps = ( ` cat $BungisFile | cut -d"=" -f1 | head -n $lmo ` )
    endif
  endif
  rm $tmp $tmp2 $tmp3  
  set is_needed = 1
  if ( $is_needed ) then
    set found_older = 0
    foreach dep ( $deps )
      tcsh -b $0 "--noclear" $dep
    end
    foreach dep ( $deps )
      if ( ! -e $dep ) then
        set date = `date +'%D(%T)' `
        printf "BUNGIS ABORT[%s]>>> '%s' DIDN'T GET BUILT \n" "$date" "$command"
      else
        if ( -M $dep > -M  $result) set found_older = 1 
      endif
    end
    if ( $found_older ) then
      set date = `date +'%D(%T)' `
      printf "BUNGIS [%s]>>> %s \n" "$date" "$command"
      eval "$command"
    endif
    @ build_cnt++
    if ( $build_cnt == $#to_build) then 
      goto fin
    endif
  endif
  next:
  @ line++
end
fin:
foreach tb ( $to_build ) 
  echo $tb >> $BungisTmp
end
if ( ! $no_clear ) rm $BungisTmp
exit
