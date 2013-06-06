/*-
 * Copyright (c) 2012 Ryan Kwolek <kwolekr2@cs.scranton.edu>. 
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

#include "main.h"
#include "vector.h"
#include "hashtable.h"
#include "mmfile.h"
#include "bptree.h"

#define NITERS 10000
#define TEST_DATA_FILE "testdata.bin"
#define TEST_DB_FILE   "test.db"


///////////////////////////////////////////////////////////////////////////////


#ifdef _WIN32

typedef LARGE_INTEGER TIMEVAL;


void TimeGetTimePrecise(TIMEVAL *ptv) {
	QueryPerformanceCounter(ptv);
}


uint32_t TimeDiffPrecise(TIMEVAL *ptv1) {
	LARGE_INTEGER freq;
	TIMEVAL tv2, *ptv2 = &tv2;

	QueryPerfomanceCounter(tv2);
	QueryPerformanceFrequency(&freq);
	return (ptv2->QuadPart - ptv1->QuadPart) * 1000000 / pfreq.QuadPart;
}

#else

typedef struct timeval TIMEVAL;


void TimeGetTimePrecise(TIMEVAL *ptv) {
	gettimeofday(ptv, NULL);
}


uint32_t TimeDiffPrecise(TIMEVAL *ptv1) {
	TIMEVAL tv2, *ptv2 = &tv2;

	gettimeofday(ptv2, NULL);
	return ((ptv2->tv_sec - ptv1->tv_sec) * 1000000) +
			(ptv2->tv_usec - ptv1->tv_usec);
}

#endif


void TestGenerateData() {
	KVPAIR kvp;
	FILE *file;
	int i;

	srand(time(NULL));
	
	file = fopen(TEST_DATA_FILE, "wb");
	if (!file) {
		fprintf(stderr, "ERROR: TestGenerateData: failed to create test file\n");
		return;
	}

	for (i = 0; i != NITERS; i++) {
		kvp.key = (float)(rand() & 0xFFFFFFF);
		kvp.val = (unsigned int)(rand() & 0xFFFFFF); 
		fwrite(&kvp, sizeof(kvp), 1, file);
	}
	
	fclose(file);
}


int KVPCompare(const void *item1, const void *item2) {
	return (int)(((LPKVPAIR)item2)->key - ((LPKVPAIR)item1)->key);
}


void TestBPTree() {
	TIMEVAL tv;
	KVPAIR testset[NITERS], kvp;
	LPKVPAIR matches;
	LPBPTREE bpt;
	float key;
	unsigned int val;
	int i, result, nparts, rcount, entriesleft;
	FILE *file;

	file = fopen(TEST_DATA_FILE, "rb");
	if (!file) {
		fprintf(stderr, "ERROR: TestBPTree: failed to open test file\n");
		return;
	}
	if (!fread(testset, sizeof(testset), 1, file)) {
		fprintf(stderr, "ERROR: TestBPTree: failed to read from test file\n");
		return;
	}
	fclose(file);
	
	bpt = BptOpen(TEST_DB_FILE);
	if (!bpt) {
		fprintf(stderr, "test: failed to open tree\n");
		return;
	}
	
	///////////////////////////////////////////////////////////////////////////  INSERTION
	TimeGetTimePrecise(&tv);
	for (i = 0; i != NITERS; i++) {
		key = testset[i].key;
		val = testset[i].val;
		if (!BptInsert(bpt, key, val)) {
			fprintf(stderr, "test: insert failed (%f, %d)\n", key, val);
			return;
		}
	}
	printf("inserted %d items, %dus\n", NITERS, TimeDiffPrecise(&tv));
	///////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////// SEARCH
	TimeGetTimePrecise(&tv);
	for (i = 0; i != NITERS; i++) {
		key = testset[i].key;
		result = BptSearch(bpt, key, &val);
		if (!result) {
			fprintf(stderr, "test: item not found (%f)\n", key);
			return;
		}
		if (result == BT_ERROR) {
			fprintf(stderr, "test: search failed (%f)\n", key);
			return;
		}
		if (val != testset[i].val) {
			fprintf(stderr, "test: search returned invalid result "
				"(%f, expected: %d, actual: %d)\n", key, testset[i].val, val);
			return;
		}
	}
	printf("searched for %d items, %dus\n", NITERS, TimeDiffPrecise(&tv));
	///////////////////////////////////////////////////////////////////////////

	qsort(testset, NITERS, sizeof(testset[0]), KVPCompare);
	printf(" min item: %f, max item: %f\n", testset[0].key, testset[NITERS - 1].key);
	
	BptGetMin(bpt, &kvp);
	if (kvp.key != testset[0].key) {
		fprintf(stderr, "test: invalid minimum key\n");
		return;
	}

	BptGetMax(bpt, &kvp);
	if (kvp.key != testset[NITERS - 1].key) {
		fprintf(stderr, "test: invalid maximum key\n");
		return;
	}

	/////////////////////////////////////////////////////////////////////////// SEARCH RANGE
	TimeGetTimePrecise(&tv);

	i = nparts = 0;
	entriesleft = NITERS;
	printf("checking ranges of size: ");
	while (entriesleft) {
		int j;

		rcount = (rand() & 0x1F) + 1;
		if (rcount > entriesleft)
			rcount = entriesleft;
		printf("%d ", rcount);

		result = BptSearchRange(bpt, testset[i].key, testset[i + rcount].key, &matches);
		if (!result) {
			fprintf(stderr, "test: range search returned no items\n");
			return;
		}
		if (result == BT_ERROR) {
			fprintf(stderr, "test: range search failed\n");
			return;
		}
		if (result != rcount) {
			fprintf(stderr, "test: range search returned invalid number "
				"of items (expected: %d, actual: %d\n", rcount, result);
			return;
		}
		for (j = 0; j != result; j++) {
			if (matches[j].key != testset[i].key ||
				matches[j].val != testset[i].val) {
				fprintf(stderr, "test: range search returned invalid item "
					"(expected: %f %d, returned: %f %d)\n",
					testset[i].key, testset[i].val, matches[j].key, matches[j].val);
				return;
			}
			i++;
		}

		free(matches);
		entriesleft -= rcount;
		nparts++;
	}
	putchar('\n');

	printf("range searched for %d items (%d calls), %dus\n", NITERS, nparts, TimeDiffPrecise(&tv));
	///////////////////////////////////////////////////////////////////////////
}

