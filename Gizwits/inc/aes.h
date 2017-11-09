#ifndef _AES_H_
#define _AES_H_
#include "tool.h"

#define AES_BLOCK_LEN       16

/*
 * Number of columns (32-bit words) comprising the State. For this
 * standard, Nb = 4.
 */
#define AES_NB_LEN      4       /* 4 */

/*
 * Number of 32-bit words comprising the Cipher Key. For this
 * standard, Nk = 4, 6, or 8.
 */
#define AES_NK_LEN      4       /* 128-bit key */

/*
 * Number of rounds, which is a function of  Nk  and  Nb (which is
 * fixed). For this standard, Nr = 10, 12, or 14.
 */
#define AES_NR_LEN      10      /* 128-bit key */

void  aesInit(void);
void  aesDestroy(void);
void sboxAssign(uint8 *outData);
void invSboxAssign(uint8 *outData);
uint32 aesECB128Encrypt(uint8 *in, uint8 *out, uint8 *key, uint32 len);
uint32 aesECB128Decrypt(uint8 *in, uint8 *out, uint8 *key, uint32 len);

#endif /* _AES_H_ */

