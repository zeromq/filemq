/*-
 * Copyright (c) 2010 Allan Saddi <allan@saddi.com>
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

#include "hmac-sha1.h"

#define HASH_BLOCK_SIZE 64

void
HMAC_SHA1_Init(HMAC_SHA1_Context *ctxt, void *key, uint32_t keyLen)
{
  SHA1_Context keyCtxt;
  int i;
  uint8_t pkey[HASH_BLOCK_SIZE], okey[HASH_BLOCK_SIZE], ikey[HASH_BLOCK_SIZE];

  /* Ensure key is zero-padded */
  memset(pkey, 0, sizeof(pkey));

  if (keyLen > sizeof(pkey)) {
    /* Hash key if > HASH_BLOCK_SIZE */
    SHA1_Init(&keyCtxt);
    SHA1_Update(&keyCtxt, key, keyLen);
    SHA1_Final(&keyCtxt, pkey);
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
  SHA1_Init(&ctxt->outer);
  SHA1_Update(&ctxt->outer, okey, sizeof(okey));
  SHA1_Init(&ctxt->inner);
  SHA1_Update(&ctxt->inner, ikey, sizeof(ikey));

  /* Burn the stack */
  memset(ikey, 0, sizeof(ikey));
  memset(okey, 0, sizeof(okey));
  memset(pkey, 0, sizeof(pkey));
  memset(&keyCtxt, 0, sizeof(keyCtxt));
}

void
HMAC_SHA1_Update(HMAC_SHA1_Context *ctxt, void *data, uint32_t len)
{
  SHA1_Update(&ctxt->inner, data, len);
}

void
HMAC_SHA1_Final(HMAC_SHA1_Context *ctxt, uint8_t hmac[SHA1_HASH_SIZE])
{
  uint8_t ihash[SHA1_HASH_SIZE];

  SHA1_Final(&ctxt->inner, ihash);
  SHA1_Update(&ctxt->outer, ihash, sizeof(ihash));
  SHA1_Final(&ctxt->outer, hmac);

  memset(ihash, 0, sizeof(ihash));
}

#ifdef HMAC_SHA1_TEST
#include <stdlib.h>
#include <stdio.h>

static void
print_hmac(uint8_t hmac[SHA1_HASH_SIZE])
{
  int i;

  for (i = 0; i < SHA1_HASH_SIZE;) {
    printf("%02x", hmac[i++]);
    if (!(i % 4)) printf(" ");
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
  HMAC_SHA1_Context ctxt;
  uint8_t hmac[SHA1_HASH_SIZE];

  memset(key1, 0x0b, sizeof(key1));
  HMAC_SHA1_Init(&ctxt, key1, sizeof(key1));
  HMAC_SHA1_Update(&ctxt, testdata1, strlen(testdata1));
  HMAC_SHA1_Final(&ctxt, hmac);

  /* Expecting: 675b0b3a 1b4ddf4e 124872da 6c2f632b fed957e9 */
  print_hmac(hmac);

  HMAC_SHA1_Init(&ctxt, key2, strlen(key2));
  HMAC_SHA1_Update(&ctxt, testdata2, strlen(testdata2));
  HMAC_SHA1_Final(&ctxt, hmac);

  /* Expecting: effcdf6a e5eb2fa2 d27416d5 f184df9c 259a7c79 */
  print_hmac(hmac);

  memset(key1, 0xaa, sizeof(key1));
  memset(testdata3, 0xdd, sizeof(testdata3));
  HMAC_SHA1_Init(&ctxt, key1, sizeof(key1));
  HMAC_SHA1_Update(&ctxt, testdata3, sizeof(testdata3));
  HMAC_SHA1_Final(&ctxt, hmac);

  /* Expecting: d730594d 167e35d5 956fd800 3d0db3d3 f46dc7bb */
  print_hmac(hmac);

  return 0;
}
#endif /* HMAC_SHA1_TEST */
