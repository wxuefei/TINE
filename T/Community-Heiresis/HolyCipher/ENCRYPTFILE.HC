// Encrypts a file

#include "CIPHER.HC";
#include "Ext/STRING.HC";

U0 Main()
{
  U8 *FileName=GetStr("Filename: ");
  U64 TextLength;
  U8 *PlainText=FileRead(FileName,&TextLength);
  U8 *Key=SecureGetStr("Enter Password");
  U64 KeyLength=StrLen(Key);
  U8 *CipherText=CAlloc(TextLength);
  F64 Start=tS;
  Encrypt(CipherText,PlainText,TextLength,Key,KeyLength);
  F64 End=tS;
  FileWrite(FileName,CipherText,TextLength);
  U8 *_FileName=StrDup(FileName);
  FileName=ReAlloc(FileName,StrLen(FileName)+4);
  StrCpy(FileName+StrLen(FileName),".ENC");
  Move(_FileName,FileName);
  // Secure erase of data
  MemSet(PlainText,0,TextLength);
  MemSet(CipherText,0,TextLength);
  MemSet(Key,0,KeyLength+1);
  MemSet(FileName,0,StrLen(FileName)+1);
  MemSet(_FileName,0,StrLen(_FileName)+1);
  VariadicFree(FileName,PlainText,CipherText,Key,_FileName);
  "Took %.7f milliseconds to encrypt\n",1000*(End-Start);
}

Main;
