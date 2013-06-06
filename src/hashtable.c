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

/* 
 * hashtable.c - 
 *    High performance hashtable routines and general purpose 32 bit hash functions
 */


#include "main.h"
#include "vector.h"
#include "hashtable.h"

uint32_t crc_tab[256];


///////////////////////////////////////////////////////////////////////////////


//set keylen to 0 for an null terminated string
LPHT HtInit(unsigned int tablelen, unsigned int keylen, int algorithm, unsigned int num_initial_slots) {
	LPHT ht;
	
	ht = malloc(sizeof(HT));
	ht->tablelen = tablelen - 1;
	ht->keylen   = keylen;
	ht->vectlen  = num_initial_slots;
	ht->table    = malloc(sizeof(LPVECTOR) * tablelen);
	memset(ht->table, 0, sizeof(LPVECTOR) * tablelen);

	switch (algorithm) {
		case HT_HASH_CRC32:
			if (!crc_tab[0])
				HtCrc32GenTab();
			ht->hash = HtCrc32Hash;
			break;
		case HT_HASH_ADLER32:
			ht->hash = HtAdler32Hash;
			break;
		case HT_HASH_DEFAULT:
			ht->hash = HtDefaultHash;
			break;
		default:
			printf("WARNING: unimplemented hash algorithm.\n");
			ht->hash = HtDefaultHash;
	}

	return ht;
}


void HtDestroy(LPHT ht) {
	free(ht);
}


void HtInsertItem(LPHT ht, const void *key, void *newentry) {
	unsigned int index;
	unsigned int keylen = ht->keylen ? ht->keylen : strlen(key);
	LPVECTOR *table = ht->table;

	index = ht->hash(key, keylen) & ht->tablelen;

	if (!table[index])
		table[index] = VectorInit(ht->vectlen);
	table[index] = VectorAdd(table[index], newentry);
}


int HtInsertItemUnique(LPHT ht, const void *key, void *newentry) {
	unsigned int index, i;
	unsigned int keylen = ht->keylen ? ht->keylen : strlen(key);
	LPVECTOR *table = ht->table;

	index = ht->hash(key, keylen) & ht->tablelen;

	if (!table[index]) {
		table[index] = VectorInit(ht->vectlen);
	} else { 
		for (i = 0; i != table[index]->numelem; i++) {
			if (!memcmp(key, table[index]->elem[i], keylen))
				return 0;
		}
	}

	table[index] = VectorAdd(table[index], newentry);
	return 1;
}


int HtRemoveItem(LPHT ht, const void *key) {
	unsigned int index, i;
	unsigned int keylen = ht->keylen ? ht->keylen : strlen(key);
	LPVECTOR *table = ht->table;
	
	index = ht->hash(key, keylen) & ht->tablelen;

	if (table[index]) {
		for (i = 0; i != table[index]->numelem; i++) {
			if (!memcmp(key, table[index]->elem[i], keylen)) {
				table[index]->numelem--;
				free(table[index]->elem[i]);
				table[index]->elem[i] = table[index]->elem[table[index]->numelem];
				return 1;
			}
		}
	}
	return 0;
}


void *HtUnassociateItem(LPHT ht, const void *key) {
	void *item;
	unsigned int index, i;
	unsigned int keylen = ht->keylen ? ht->keylen : strlen(key);
	LPVECTOR *table = ht->table;
	
	index = ht->hash(key, keylen) & ht->tablelen;

	if (table[index]) {
		for (i = 0; i != table[index]->numelem; i++) {
			if (!memcmp(key, table[index]->elem[i], keylen)) {
				table[index]->numelem--;
				item = table[index]->elem[i];
				table[index]->elem[i] = table[index]->elem[table[index]->numelem];
				return item;
			}
		}
	}
	return NULL;
}


void *HtGetItem(LPHT ht, const void *key) {
	unsigned int index, i;
	unsigned int keylen = ht->keylen ? ht->keylen : strlen(key);
	LPVECTOR *table = ht->table;

	index = ht->hash(key, keylen) & ht->tablelen;

	if (table[index]) {
		for (i = 0; i != table[index]->numelem; i++) {
			if (!memcmp(key, table[index]->elem[i], keylen))
				return table[index]->elem[i];
		}
	}
	return NULL;
}


void HtResetContents(LPHT ht) {
	unsigned int i;
	LPVECTOR *table = ht->table;

	for (i = 0; i != ht->tablelen; i++) {
		if (table[i]) {
			VectorDelete(table[i]);
			table[i] = NULL;
		}
	}
}


uint32_t HtDefaultHash(const void *key, unsigned int len) {
    uint32_t hash = 0;
	const unsigned char *k = key;

	while (len--) {
		hash += *k;
		hash += (hash << 10);
		hash ^= (hash >> 6);
		k++;
	}
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}


void HtCrc32GenTab() {
	uint32_t crc;
	int i, j;

	for (i = 0; i != 256; i++) {
		crc = i;
		for (j = 8; j > 0; j--)
			crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320L : crc >> 1;
		crc_tab[i] = crc;
	}
}


uint32_t HtCrc32Hash(const void *key, unsigned int len) {
	unsigned int i;
	uint32_t crc;
	const unsigned char *k = key;

	crc = 0xFFFFFFFF;
	for (i = 0; i != len; i++)
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *k++) & 0xFF];

	return (crc ^ 0xFFFFFFFF);
}


uint32_t HtAdler32Hash(const void *key, unsigned int len) {
	unsigned int i;
	uint32_t a = 1, b = 0;

	for (i = 0; i < len; i++) {
		a = (a + ((unsigned char *)key)[i]) % 65521;
		b = (b + a) % 65521;
	}

	return (b << 16) | a;
}

