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

#include "hmac-sha512.h"

#define HASH_BLOCK_SIZE 128

void
HMAC_SHA512_Init(HMAC_SHA512_Context *ctxt, void *key, uint32_t keyLen)
{
  SHA512_Context keyCtxt;
  int i;
  uint8_t pkey[HASH_BLOCK_SIZE], okey[HASH_BLOCK_SIZE], ikey[HASH_BLOCK_SIZE];

  /* Ensure key is zero-padded */
  memset(pkey, 0, sizeof(pkey));

  if (keyLen > sizeof(pkey)) {
    /* Hash key if > HASH_BLOCK_SIZE */
    SHA512_Init(&keyCtxt);
    SHA512_Update(&keyCtxt, key, keyLen);
    SHA512_Final(&keyCtxt, pkey);
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
  SHA512_Init(&ctxt->outer);
  SHA512_Update(&ctxt->outer, okey, sizeof(okey));
  SHA512_Init(&ctxt->inner);
  SHA512_Update(&ctxt->inner, ikey, sizeof(ikey));

  /* Burn the stack */
  memset(ikey, 0, sizeof(ikey));
  memset(okey, 0, sizeof(okey));
  memset(pkey, 0, sizeof(pkey));
  memset(&keyCtxt, 0, sizeof(keyCtxt));
}

void
HMAC_SHA512_Update(HMAC_SHA512_Context *ctxt, void *data, uint32_t len)
{
  SHA512_Update(&ctxt->inner, data, len);
}

void
HMAC_SHA512_Final(HMAC_SHA512_Context *ctxt, uint8_t hmac[SHA512_HASH_SIZE])
{
  uint8_t ihash[SHA512_HASH_SIZE];

  SHA512_Final(&ctxt->inner, ihash);
  SHA512_Update(&ctxt->outer, ihash, sizeof(ihash));
  SHA512_Final(&ctxt->outer, hmac);

  memset(ihash, 0, sizeof(ihash));
}

#ifdef HMAC_SHA512_TEST
#include <stdlib.h>
#include <stdio.h>

static void
print_hmac(uint8_t hmac[SHA512_HASH_SIZE])
{
  int i;

  for (i = 0; i < SHA512_HASH_SIZE;) {
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
  HMAC_SHA512_Context ctxt;
  uint8_t hmac[SHA512_HASH_SIZE];

  memset(key1, 0x0b, sizeof(key1));
  HMAC_SHA512_Init(&ctxt, key1, sizeof(key1));
  HMAC_SHA512_Update(&ctxt, testdata1, strlen(testdata1));
  HMAC_SHA512_Final(&ctxt, hmac);

  /* Expecting:
   * 7641c48a 3b4aa8f8 87c07b3e 83f96aff b89c978f ed8c96fc bbf4ad59 6eebfe49
   * 6f9f16da 6cd080ba 393c6f36 5ad72b50 d15c71bf b1d6b81f 66a91178 6c6ce932
   */
  print_hmac(hmac);

  HMAC_SHA512_Init(&ctxt, key2, strlen(key2));
  HMAC_SHA512_Update(&ctxt, testdata2, strlen(testdata2));
  HMAC_SHA512_Final(&ctxt, hmac);

  /* Expecting:
   * 164b7a7b fcf819e2 e395fbe7 3b56e0a3 87bd6422 2e831fd6 10270cd7 ea250554
   * 9758bf75 c05a994a 6d034f65 f8f0e6fd caeab1a3 4d4a6b4b 636e070a 38bce737
   */
  print_hmac(hmac);

  memset(key1, 0xaa, sizeof(key1));
  memset(testdata3, 0xdd, sizeof(testdata3));
  HMAC_SHA512_Init(&ctxt, key1, sizeof(key1));
  HMAC_SHA512_Update(&ctxt, testdata3, sizeof(testdata3));
  HMAC_SHA512_Final(&ctxt, hmac);

  /* Expecting:
   * ad9b5c7d e7269373 7cd5e9d9 f41170d1 8841fec1 201c1c1b 02e05cae 11671800
   * 9f771cad 9946ddbf 7e3cde3e 818d9ae8 5d91b2ba dae94172 d096a44a 79c91e86
   */
  print_hmac(hmac);

  return 0;
}
#endif /* HMAC_SHA512_TEST */
