// Decrypts a file. Are you retarded?

#include "CIPHER.HC";
#include "Ext/STRING.HC";

U0 Main()
{
  U8 *FileName=GetStr("Filename: ");
  if(StrLen(FileName)<5||StrCmp(FileName+StrLen(FileName)-4,".ENC"))
    goto FAIL;
  U64 TextLength;
  U8 *Key=SecureGetStr("Enter Password");
  U64 KeyLength=StrLen(Key);
  U8 *CipherText=FileRead(FileName,&TextLength);
  U8 *DecryptedText=CAlloc(TextLength);
  F64 Start=tS;
  Decrypt(CipherText,DecryptedText,TextLength,Key,KeyLength);
  F64 End=tS;
  FileWrite(FileName,DecryptedText,TextLength);
  U8 *_FileName=StrDup(FileName);
  _FileName[StrLen(_FileName)-4]=0;
  Move(FileName,_FileName);
  // Secure removal of data
  MemSet(CipherText,0,++TextLength);
  MemSet(DecryptedText,0,TextLength);
  MemSet(Key,0,KeyLength+1);
  MemSet(FileName,0,StrLen(FileName+1));
  MemSet(_FileName,0,StrLen(FileName)+6);
  VariadicFree(CipherText,DecryptedText,Key,FileName,_FileName);
  "Took %.7f milliseconds to decrypt\n",1000*(End-Start);
  goto END;
FAIL:
  "Not a valid .ENC file\n";
END:
}

Main;
