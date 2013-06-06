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

#ifndef THUMB_HEADER
#define THUMB_HEADER

#define THUMBCACHE_INITIAL_LEN sizeof(TCHEADER)
#define TC_MTIME_DELETED 0

#define TC_DUMP_NONE 0
#define TC_DUMP_INFO 1
#define TC_DUMP_IMGS 2

// >10MB would be a little too big for a 64x64 PNG image...
#define THUMB_MAX_SIZE (10 * 1024 * 1024)

/*
 * Thumb Cache File Format:
 *
 * [UINT32] 'TMBC' signature
 * [time_t] timestamp of directory's recorded last update
 * For each entry:
 *     [time_t]  date image was last modified
 *     [UINT32]  thumbnail data size
 *     [FLOAT]   thumbnail color key
 *     [UINT8]   filename length
 *     [CHAR []] filename
 *     [void]    image thumbnail data
 */

//#pragma pack(push, 1)

typedef struct _tcheader {
	uint32_t signature;
	time_t lastupdate;
} TCHEADER, *LPTCHEADER;

typedef struct _tcentry {
	time_t mtime;
	unsigned char fnlen;
	uint32_t thumbfsize;
	float thumbkey;
	char filename[0];
} TCENTRY, *LPTCENTRY;

//#pragma pack(pop)

typedef struct _tcrecord {
	unsigned int offset;
	TCENTRY ent;
} TCRECORD, *LPTCRECORD;

extern char thumb_btree_fn[256];
extern char thumb_cache_fn[256];
extern int burstmode;


int ThumbCacheBurstReadBegin(int reinit);
int ThumbCacheBurstReadEnd();

gdImagePtr ThumbCreate(const char *filename, unsigned int *filesize);

void ThumbCacheEnumerate(int level);
int ThumbCacheUpdate();
int ThumbFindMatches(const char *filename, LPTCENTRY *dupents,
					 unsigned int *dupoffs, unsigned int nmaxdups);

int ThumbCacheAdd(FILE *tc, const char *filename, time_t mtime);
int ThumbCacheReplace(FILE *tc, const char *filename, LPTCRECORD ptcrec, time_t mtime);
int ThumbCacheRemove(unsigned int offset);
int ThumbCacheGet(int nitems, unsigned int *offsets,
				  LPTCENTRY *entries, gdImagePtr *thumbs);
LPTCENTRY ThumbCacheLookup(unsigned int offset);
int ThumbCacheFlush();

float _ThumbCalcKey(int **tpixels);
void _ThumbFlatten(int **tpixels, int mask);
void _ThumbCacheBuildHt(FILE *tc);
unsigned int _ThumbCacheWriteEntry(FILE *tc, LPTCENTRY ptcent,
						  const char *filename, void *thumbdata);
int _ThumbCacheUpdateStructures(const char *filename, LPTCENTRY ptcent,
								unsigned int offset, int update);
void _ThumbCacheUpdateDirScan(FILE *tc, const char *dir);

#endif //THUMB_HEADER

