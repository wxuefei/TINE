// Dealing with strings and more.

#ifndef _STRING_HC
#define _STRING_HC

U8 *StrChr(U8 *Str,U8 Pivot)
{
  //U8 *Orig=Str;
  while (*Str!=Pivot&&*Str!=0)Str++;
  if (*Str==Pivot) return Str;
  else return 0;
}

Bool StrCmp2(U8 *x,U8 *y)
{
  while (*x++&&*y++)
    if (*x!=*y)
      return FALSE;
  return (*y==0);
}

U8 *StrStr(U8 *Pool,U8 *Query)
{
  while (*Pool!=0) {
    if((*Pool==*Query)&&StrCmp2(Pool,Query))
      return Pool;
    Pool++;
  }
  return NULL;
}

Bool StrContains(U8 *str,U8 *sfind)
{
  U64 len=StrLen(str),index=0,found=0;
  if (StrLen(sfind)>len)
    return FALSE;
  while (index<len) {
    if (str[index]==sfind[found]) {
      found++;
      if (StrLen(sfind)==found)
        return TRUE;
    }
    else found=0;
    index++;
  }
  return FALSE;
}

U8 *SecureGetStr(U8 *s=NULL,Bool Clear=FALSE)
{
  U8 *ret;
  "%s\n",s;
  if (!Clear)
    "$$ID$$$$TR,\"Password Hidden, Do Not Click\"$$\n"; // Ctrl+t at DolDoc help index for a hint
  else "$$ID$$$$TR,\"Password Hidden, Do Not Click, Scrn will be cleared after input\"$$\n";
  "$$ID,20000$$";
  // Free this shit afterwards
  ret=GetStr;
  "$$ID$$";
  if (Clear) "$$CL$$";
  return ret;
}

U8 *ReAlloc(U8 *p,U64 sz,CTask *m=NULL)
{ // Proudly stolen from ZealOS
  // It's public domain after all
  U8 *r; // ret
  if (!sz) {
    r=NULL;
    goto RET_;
  }
  r=CAlloc(sz,m);
  if (!p) goto RET_; // Free(0) is valid)
  MemCpy(r,p,MinI64(MSize(p),sz));
RET_:
  Free(p); // OrigFree
  return r;
}

U8 *MemDup(U8 *p)
{
  U8 *r;
  if (!p) {
    r=NULL;
    goto RET_;
  }
  r=CAlloc(MSize(p));
  MemCpy(r,p,MSize(p));
RET_:
  return r;
}

U8 *StrDup(U8 *p)
{ // WEOW!!!! I DONT HAVE TO DEFINE X_OPEN_SOURCE OR
  // WHATEVER THE FUCK THAT SHIT IS!!!!!!!!!!!!!!!!!
  U8 *r;
  if (!p) {
    r=NULL;
    goto RET_;
  }
  r=CAlloc(MSize(p));
  StrCpy(r,p);
RET_:
  return r;
}

#endif // _STRING_HC
