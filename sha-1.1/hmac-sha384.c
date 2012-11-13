/*-
 * Copyright (c) 2010, 2011 Allan Saddi <allan@saddi.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif
#include <string.h>

#include "hmac-sha384.h"

#define HASH_BLOCK_SIZE 128

void
HMAC_SHA384_Init(HMAC_SHA384_Context *ctxt, void *key, uint32_t keyLen)
{
  SHA384_Context keyCtxt;
  int i;
  uint8_t pkey[HASH_BLOCK_SIZE], okey[HASH_BLOCK_SIZE], ikey[HASH_BLOCK_SIZE];

  /* Ensure key is zero-padded */
  memset(pkey, 0, sizeof(pkey));

  if (keyLen > sizeof(pkey)) {
    /* Hash key if > HASH_BLOCK_SIZE */
    SHA384_Init(&keyCtxt);
    SHA384_Update(&keyCtxt, key, keyLen);
    SHA384_Final(&keyCtxt, pkey);
  }
  else {
    memcpy(pkey, key, keyLen);
  }

  /* XOR with opad, ipad */
  for (i = 0; i < sizeof(okey); i++) {
    okey[i] = pkey[i] ^ 0x5c;
  }
  for (i = 0; i < sizeof(ikey); i++) {
    ikey[i] = pkey[i] ^ 0x36;
  }

  /* Initialize hash contexts */
  SHA384_Init(&ctxt->outer);
  SHA384_Update(&ctxt->outer, okey, sizeof(okey));
  SHA384_Init(&ctxt->inner);
  SHA384_Update(&ctxt->inner, ikey, sizeof(ikey));

  /* Burn the stack */
  memset(ikey, 0, sizeof(ikey));
  memset(okey, 0, sizeof(okey));
  memset(pkey, 0, sizeof(pkey));
  memset(&keyCtxt, 0, sizeof(keyCtxt));
}

void
HMAC_SHA384_Update(HMAC_SHA384_Context *ctxt, void *data, uint32_t len)
{
  SHA384_Update(&ctxt->inner, data, len);
}

void
HMAC_SHA384_Final(HMAC_SHA384_Context *ctxt, uint8_t hmac[SHA384_HASH_SIZE])
{
  uint8_t ihash[SHA384_HASH_SIZE];

  SHA384_Final(&ctxt->inner, ihash);
  SHA384_Update(&ctxt->outer, ihash, sizeof(ihash));
  SHA384_Final(&ctxt->outer, hmac);

  memset(ihash, 0, sizeof(ihash));
}

#ifdef HMAC_SHA384_TEST
#include <stdlib.h>
#include <stdio.h>

static void
print_hmac(uint8_t hmac[SHA384_HASH_SIZE])
{
  int i;

  for (i = 0; i < SHA384_HASH_SIZE;) {
    printf("%02x", hmac[i++]);
    if (i == 32)
      printf("\n");
    else if (!(i % 4))
      printf(" ");
  }
  printf("\n");
}

int
main(int argc, char *argv[])
{
  uint8_t key1[16];
  char *key2 = "Jefe";
  char *testdata1 = "Hi There";
  char *testdata2 = "what do ya want for nothing?";
  uint8_t testdata3[50];
  HMAC_SHA384_Context ctxt;
  uint8_t hmac[SHA384_HASH_SIZE];

  memset(key1, 0x0b, sizeof(key1));
  HMAC_SHA384_Init(&ctxt, key1, sizeof(key1));
  HMAC_SHA384_Update(&ctxt, testdata1, strlen(testdata1));
  HMAC_SHA384_Final(&ctxt, hmac);

  /* Expecting:
   * 7afaa633 e20d379b 02395915 fbc385ff 8dc27dcd 3885e106 8ab942ee ab52ec1f
   * 20ad382a 92370d8b 2e0ac8b8 3c4d53bf 
   */
  print_hmac(hmac);

  HMAC_SHA384_Init(&ctxt, key2, strlen(key2));
  HMAC_SHA384_Update(&ctxt, testdata2, strlen(testdata2));
  HMAC_SHA384_Final(&ctxt, hmac);

  /* Expecting:
   * af45d2e3 76484031 617f78d2 b58a6b1b 9c7ef464 f5a01b47 e42ec373 6322445e
   * 8e2240ca 5e69e2c7 8b3239ec fab21649 
   */
  print_hmac(hmac);

  memset(key1, 0xaa, sizeof(key1));
  memset(testdata3, 0xdd, sizeof(testdata3));
  HMAC_SHA384_Init(&ctxt, key1, sizeof(key1));
  HMAC_SHA384_Update(&ctxt, testdata3, sizeof(testdata3));
  HMAC_SHA384_Final(&ctxt, hmac);

  /* Expecting:
   * 1383e82e 28286b91 f4cc7afb d13d5b5c 6f887c05 e7c45424 84043a37 a5fe4580
   * 2a9470fb 663bd7b6 570fe2f5 03fc92f5 
   */
  print_hmac(hmac);

  return 0;
}
#endif /* HMAC_SHA384_TEST */
