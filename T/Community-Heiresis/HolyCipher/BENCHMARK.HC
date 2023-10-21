// CYCLE_NUM cycles and 1000 chars.
// Is your computer fast enough?

#include "CIPHER.HC";

#define CYCLE_NUM 10000

U0 Main()
{
  U8 *PlainText="This text will become a jumble of garbageDoes it matter? The only way you can ever use gets istter? The only way you can ever use gets is if stdin is known to be attached to a file whose contents you havetter? The only way you can ever use gets is if std
in is known to be attached to a file whose contents you have if stdin is known to be attached to a file whose contents you have full control over. This condition is almost impossible to satisfy, especially on multiprocess systems where other processes may modify files as
ynchronously with respect to your program. Therefore, for all practical purposes, any program using gets has undefined behavior (i.e. there are possible inputs/environmental conditions for which it will have undefined behavior), and in particular UB which is likely to le
ad to privilege compromise if your program has higher privileges than the provider of the dtter? The only way you can ever use gets is if stdin ihose contents you haveata.y one I can think of right off...";
  U64 TextLength=StrLen(PlainText);
  U8 *Key="WowAVeryStrongPassword";
  U64 KeyLength=StrLen(Key);
  U8 *CipherText=CAlloc(TextLength+1);
  U8 *DecryptedText=CAlloc(TextLength+1);
  "Length of plaintext/ciphertext: %lu\n\n",TextLength;

  // I hate for loops, fuck Dijkstra
  U64 cycles=0;
  F64 Start=tS;
CYCLE_:
  if (!(cycles++<CYCLE_NUM))
    goto BREAK_;

  Encrypt(CipherText,PlainText,TextLength,Key,KeyLength);

  //DumpNumerical(CipherText,PlainText,TextLength);
  //'\n';
  /*DumpRaw(CipherText,TextLength);
  '\n';*/

  Decrypt(CipherText,DecryptedText,TextLength,Key,KeyLength);

  //DumpRaw(CipherText,TextLength);
  //'\n';

  goto CYCLE_;
BREAK_:
  F64 End=tS;
  DumpNumerical(CipherText,PlainText,TextLength);
  if (!StrCmp(PlainText,DecryptedText))
    "Original text and deciphered text are equal\n";
  VariadicFree(CipherText,DecryptedText);
  "%lu chars, %lu cycles, elapsed time: %.7f milliseconds\n",TextLength,CYCLE_NUM,1000*(End-Start);
}

Main;
