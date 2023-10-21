// Simple XOR-Shift Cipher(currently, might add more for security)
// Currently input string and output(encrypted string) have the same length
// TODO: Add salt, More complicated shifting techniques (I first have to learn more)

#ifndef CIPHER_HC
#define CIPHER_HC

U0 XORCipher(U8 *res=NULL,U8 *text=NULL,U64 textlen=0,U8 *key=NULL,U64 keylen=0)
{
  U64 i=0;
  for (; i<textlen; i++)
    res[i]=text[i]^key[i%keylen];
}

U0 OutShift(U8 *res=NULL,U64 textlen=0,U64 shift=0)
{
  U64 i=0;
  for (; i<textlen; i++)
    res[i]=((res[i]<<(8-shift))
           |(res[i]>>shift));
}

U0 InShift(U8 *res=NULL,U64 textlen=0,U64 shift=0)
{
  U64 i=0;
  for (; i<textlen; i++)
    res[i]=((res[i]>>(8-shift))
           |(res[i]<<shift));
}

U0 DumpNumerical(U8 *str=NULL,U8 *origstr=NULL,U64 textlen=0)
{
  U64 i=0;
  for (; i<textlen; i++)
    "%lu. %u - %c\t",i+1,str[i],origstr[i];
  '\n';
}

U0 DumpRaw(U8 *str=NULL,U64 textlen=0)
{
  U64 i=0;
  for (; i<textlen; i++)
    // Normally I don't need to call PutChars()
    // when printing a char(just '\n') but this
    // is a value and the compiler doesn't know if
    // it's a char
    PutChars(str[i%textlen]);
  '\n';
}


U0 Encrypt(U8 *ciphertext=NULL,U8 *plaintext=NULL,U64 textlen=0,U8 *key=NULL,U64 keylen=0)
{
  XORCipher(ciphertext,plaintext,textlen,key,keylen);
  OutShift(ciphertext,textlen,7);
  XORCipher(ciphertext,ciphertext,textlen,key,keylen);
  InShift(ciphertext,textlen,2);

  XORCipher(ciphertext,ciphertext,textlen,key,keylen);
  OutShift(ciphertext,textlen,3);
  XORCipher(ciphertext,ciphertext,textlen,key,keylen);
  InShift(ciphertext,textlen,1);
  XORCipher(ciphertext,ciphertext,textlen,key,keylen);
}

U0 Decrypt(U8 *ciphertext=NULL,U8 *decryptedtext=NULL,U64 textlen=0,U8 *key=NULL,U64 keylen=0)
{
  MemCpy(decryptedtext,ciphertext,textlen);

  XORCipher(decryptedtext,decryptedtext,textlen,key,keylen);
  OutShift(decryptedtext,textlen,1);
  XORCipher(decryptedtext,decryptedtext,textlen,key,keylen);
  InShift(decryptedtext,textlen,3);
  XORCipher(decryptedtext,decryptedtext,textlen,key,keylen);

  OutShift(decryptedtext,textlen,2);
  XORCipher(decryptedtext,decryptedtext,textlen,key,keylen);
  InShift(decryptedtext,textlen,7);
  XORCipher(decryptedtext,decryptedtext,textlen,key,keylen);
}

U0 VariadicFree(...)
{ // Free(), but variadic
  U64 i=0;
  for (; i<argc; i++)
    Free(argv[i]);
}

#endif
