/*-
 * Copyright (c) 2008 Ryan Kwolek <kwolekr2@cs.scranton.edu>. 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other materials
 *     provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HASHTABLE_HEADER
#define HASHTABLE_HEADER

/////////// Compile-time configuration ////////////
#define HT_INITIAL_SLOTS 2
#define HT_CASE_INSENSITIVE
///////////////////////////////////////////////////

#include "vector.h"

#ifndef HT_CASE_INSENSITIVE
	#define strilcmp(x,y) strcmp(x,y)
	#define __key key
#endif

#define HT_HASH_CRC32   0
#define HT_HASH_ADLER32 1
#define HT_HASH_DEFAULT 2

typedef struct _ht {
	LPVECTOR *table;
	unsigned int tablelen;
	unsigned int keylen;
	unsigned int vectlen;

	uint32_t (*hash)(const void *, unsigned int);
} HT, *LPHT;

LPHT HtInit(unsigned int tablelen, unsigned int keylen, int algorithm, unsigned int num_initial_slots);
void HtInsertItem(LPHT ht, const void *key, void *newentry);
int HtInsertItemUnique(LPHT ht, const void *key, void *newentry);
int HtRemoveItem(LPHT ht, const void *key);
void *HtUnassociateItem(LPHT ht, const void *key);
void *HtGetItem(LPHT ht, const void *key);
void HtResetContents(LPHT ht);
uint32_t HtDefaultHash(const void *key, unsigned int len);
void HtCrc32GenTab();
uint32_t HtCrc32Hash(const void *key, unsigned int len);
uint32_t HtAdler32Hash(const void *key, unsigned int len);

#endif //HASHTABLE_HEADER

