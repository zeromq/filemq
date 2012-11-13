/*-
 * Copyright (c) 2001-2003 Allan Saddi <allan@saddi.com>
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

#ifndef _APS_SHA512_H
#define _APS_SHA512_H

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#define SHA512_HASH_SIZE 64
#define SHA512t224_HASH_SIZE 28
#define SHA512t256_HASH_SIZE 32

/* Hash size in 64-bit words */
#define SHA512_HASH_WORDS 8
#define SHA512t224_HASH_WORDS 3/*.5*/
#define SHA512t256_HASH_WORDS 4

typedef struct _SHA512_Context {
  uint64_t totalLength[2];
  uint64_t hash[SHA512_HASH_WORDS];
  uint32_t bufferLength;
  union {
    uint64_t words[16];
    uint8_t bytes[128];
  } buffer;
#ifdef RUNTIME_ENDIAN
  int littleEndian;
#endif /* RUNTIME_ENDIAN */
} SHA512_Context;

#ifdef __cplusplus
extern "C" {
#endif

void SHA512_Init (SHA512_Context *sc);
void SHA512_Update (SHA512_Context *sc, const void *data, uint32_t len);
void SHA512_Final (SHA512_Context *sc, uint8_t hash[SHA512_HASH_SIZE]);

/* SHA-512/224 and SHA-512/256 additions. */
void SHA512t224_Init (SHA512_Context *sc);
#define SHA512t224_Update SHA512_Update
void SHA512t224_Final (SHA512_Context *sc, uint8_t hash[SHA512t224_HASH_SIZE]);

void SHA512t256_Init (SHA512_Context *sc);
#define SHA512t256_Update SHA512_Update
void SHA512t256_Final (SHA512_Context *sc, uint8_t hash[SHA512t256_HASH_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* !_APS_SHA512_H */
