//Created by ntcp2
U64 FNV64(U8 *s)
{
  U64 hash=0xCBF29CE484222325;
  while (*s) {
    hash*=0x100000001B3;
    hash^=*s++;
  }
  return hash;
}

U8 *FNV64Str(U8 *s)
{
  U8 *ret=CAlloc(17);
  U64 res=FNV64(s);
  StrPrintHex(ret,res,16);
  return ret;
}

#ifdef FNV64_TEST
if (FNV64("a")!=0xAF63BD4C8601B7BE
  ||StrCmp(FNV64Str("a"),"AF63BD4C8601B7BE"))
  throw('ERR_FNV');
#endif 

/*
U8 *HashPW(U8 *s)
{
  if(!*s) return 0;
  U8 str[4097];
  MemSet(str,0,4097);
  U64 len=StrLen(s);
  if (len>=4096)
    MemCpy(str,s,4096);
  else
  {
    U64 i;
    for (i=0;i<4096/len;++i)
      MemCpy(str+len*i,s,len);
  }
  I64 res=HashStr(str);
  U8 *ret=CAlloc(17);
  StrPrintHex(ret,res,16);
  return ret;
}
//"%s\n%s\n%s\n",HashPW("abc"),HashPW("abc"),HashPW("abc");
*/
