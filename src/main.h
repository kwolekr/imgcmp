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

#ifndef MAIN_HEADER
#define MAIN_HEADER

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include <gd.h>
#include <gdfontt.h>
#include <gdfonts.h>
#include <gdfontmb.h>
#include <gdfontl.h>

#ifdef _WIN32
#	pragma warning(disable : 4731) //stupid warning about ebp being modified
#	pragma warning(disable : 4305) //stupid warning about int->char truncation
#	pragma warning(disable : 4996) //stupid warning about POSIX being deprecated

#	include <windows.h>
#	include <direct.h>

	typedef __int8 int8_t;
	typedef __int16 int16_t;
	typedef __int32 int32_t;
	typedef __int64 int64_t;
	typedef unsigned __int8 uint8_t;
	typedef unsigned __int16 uint16_t;
	typedef unsigned __int32 uint32_t;
	typedef unsigned __int64 uint64_t;

#	define restrict
#	define inline _inline

#	define printerr(x) fprintf(stderr, "%s: error %d\n", (x), GetLastError())
#	define IsAbsolutePath(x) ((x)[0] && (x)[1] == ':' && (x)[2] == '\\')
#	define createdir(x) mkdir(x)
#	define snprintf sprintf_s
#	define alloca(x) _alloca(x)

#	define SWAP16(x) _byteswap_ushort(x)
#	define SWAP32(x) _byteswap_ulong(x)
#	define SWAP64(x) _byteswap_uint64(x)
#else
#	include <unistd.h>
#	include <fcntl.h>
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <sys/mman.h>
#	include <dirent.h>
#	include <limits.h>

#	if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#		define ENDIAN_BIG
#	endif

#	define printerr(x) perror(x)
#	define IsAbsolutePath(x) ((x)[0] == '/')
#	define createdir(x) mkdir(x, S_IRWXU | S_IRWXG | S_IRWXO)
#	define MAX_PATH 256

//#	define SWAP16(x) __builtin_bswap16(x)
//#	define SWAP32(x) __builtin_bswap32(x)
//#	define SWAP64(x) __builtin_bswap64(x)
#define SWAP16(num) ((((num) >> 8) & 0x00FF) | \
					(((num) << 8) & 0xFF00))
#define SWAP32(num) ((((num) >> 24) & 0x000000FF) | \
					(((num) >> 8) & 0x0000FF00) | \
					(((num) << 8) & 0x00FF0000) | \
					(((num) << 24) & 0xFF000000))

#endif

#ifdef ENDIAN_BIG
#	define LE16(x) SWAP16(x)
#	define LE32(x) SWAP32(x)
#	define LE64(x) SWAP64(x)
#	define BE16(x) (x)
#	define BE32(x) (x)
#	define BE64(x) (x)
#else
#	define LE16(x) (x)
#	define LE32(x) (x)
#	define LE64(x) (x)
#	define BE16(x) SWAP16(x)
#	define BE32(x) SWAP32(x)
#	define BE64(x) SWAP64(x)
#endif

#ifdef NEED_ALIGNMENT

inline uint16_t UAR16(void *addr) {
	uint16_t result;
	memcpy(&result, addr, sizeof(result));
	return result;
}

inline uint32_t UAR32(void *addr) {
	uint32_t result;
	memcpy(&result, addr, sizeof(result));
	return result;
}

inline uint64_t UAR64(void *addr) {
	uint64_t result;
	memcpy(&result, addr, sizeof(result));
	return result;
}

inline void UAW16(void *addr, uint16_t data) {
	memcpy(addr, &data, sizeof(data));
}

inline void UAW32(void *addr, uint32_t data) {
	memcpy(addr, &data, sizeof(data));
}

inline void UAW64(void *addr, uint64_t data) {
	memcpy(addr, &data, sizeof(data));
}

#else
#	define UAR16(x) (*(uint16_t *)(x))
#	define UAR32(x) (*(uint32_t *)(x))
#	define UAR64(x) (*(uint64_t *)(x))
#	define UAW16(x,y) (*(uint16_t *)(x) = (y))
#	define UAW32(x,y) (*(uint32_t *)(x) = (y))
#	define UAW64(x,y) (*(uint64_t *)(x) = (y))
#endif

#define ALIGN_BYTES (sizeof(int))
#define ALIGN_MASK (ALIGN_BYTES - 1)

#define THUMB_CX 64
#define THUMB_CY 64
#define THUMB_NPIXELS (THUMB_CX * THUMB_CY)

#define DIFF_TOLERANCE 1.5f

#define ARRAYLEN(x) (sizeof(x) / sizeof((x)[0]))

#ifdef _WIN32
#	define PATH_SEPARATOR '\\'
#else
#	define PATH_SEPARATOR '/'
#endif

#define TEXT_USAGE "usage: imgcmp <not done yet>"
#define TEXT_VERSION "version here"

#define IMG_CMP_NONE    0
#define IMG_CMP_ABS     1
#define IMG_CMP_RANGE   2
#define IMG_CMP_HISTRGB 3
#define IMG_CMP_HISTHSV 4
#define IMG_CMP_PHASH   5

int scan_recursive;
int verbose;

char workdir[256], outpath[256];


void ParseCmdLine(int argc, char *argv[]);

void ImageComparisonPerform(int method, const char *f1, const char *f2);

int BuildPath(const char *filename);

time_t GetLastWriteTime(const char *filename);

#ifdef _WIN32
	time_t FileTimeToUnixTime(FILETIME ft);
	void UnixTimeToFileTime(time_t t, LPFILETIME pft);
#endif

size_t strrplcpy(char *__restrict dst, const char *__restrict src,
				 size_t len, const char find, const char replacewith);
char *stpcpy(char *__restrict dst, const char *__restrict src);
#ifndef __BSD_VISIBLE
	size_t strlcpy(char *__restrict dst, const char *__restrict src, size_t len);
#endif

#endif //MAIN_HEADER

