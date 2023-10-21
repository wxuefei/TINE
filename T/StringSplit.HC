U0 Split(U8 *String){
  String = MStrPrint("%s ", String);
  I64 count = 3;
  U8 *full="";
  U8 *tmpString = "";
  U8 tmpBuf, oldBuf, i;

  U8 **str_arr = MAlloc(sizeof(U8*)*count);

  for (i = 0; i < StrLen(String); i++){
    tmpBuf = String[i];
    if (tmpBuf != ' '){
      tmpString = MStrPrint("%s%c", tmpString, tmpBuf);
    } else{
      str_arr[i] = CAlloc(StrLen(tmpString)+1);
      StrCpy(str_arr[i], tmpString);
      tmpString = "";
    }
    oldBuf=tmpBuf;
  }
  "%s", str_arr[1];

}

Split("TEST STRING SPLIT");