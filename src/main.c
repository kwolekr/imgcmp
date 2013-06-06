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

/*
 * TODO:
 *
 *    bptree.c:
 *    - implement bin drawing in BptDraw
 *    - make value comparison into a macro
 *
 *    main.c:
 *    - implement the rest of the fucking program
 *
 *    test.c
 *    - create this!
 *    - add unit tests for bptree, hashtable, vector, mmfile, so on.
 *
 *    thumb.c
 *    - Fix up the assembly implementations of ThumbCalcKey
 *
 */

#include "main.h"
#include "vector.h"
#include "hashtable.h"
#include "mmfile.h"
#include "bptree.h"
#include "img.h"
#include "thumb.h"
#include "dedup.h"

int verbose;
int comparison, deduplicate_dir, scan_recursive;
int npixels_diff, pixel_tolerance;
int cache_no_update, cache_flush, cache_dont_use, cache_dump;
char workdir[256];
char outpath[256];
char imgpath1[256], imgpath2[256];

void TestGenerateData();
void TestBPTree();


///////////////////////////////////////////////////////////////////////////////


int main(int argc, char *argv[]) {
#ifdef RUN_UNIT_TESTS
	TestGenerateData();
	TestBPTree();
	return 0;
#endif

	ParseCmdLine(argc, argv);

	if (workdir[0]) {
		if (chdir(workdir) == -1) {
			perror("chdir");
			return 0;
		}
		if (verbose)
			printf(" >> Set CWD to %s\n", workdir);
	}

	if (cache_flush) {
		ThumbCacheFlush();
		return 0;
	}
	
	if (!cache_no_update && !cache_dont_use)
		ThumbCacheUpdate();

	if (comparison)
		ImageComparisonPerform(comparison, imgpath1, imgpath2);
	if (cache_dump)
		ThumbCacheEnumerate(cache_dump);
	if (deduplicate_dir)
		DedupPerform(workdir);

	return 0;
}


#define CACHE_CMD_SETINDEX 0
#define CACHE_CMD_SETDATA  1
#define CACHE_CMD_DUMPALL  2
#define CACHE_CMD_DUMPINFO 3
#define CACHE_CMD_DISABLE  4
#define CACHE_CMD_NOUPDATE 5

const char *cache_cmd_strs[] = {
	"setindex",
	"setdata",
	"dumpall",
	"dumpinfo",
	"disable",
	"noupdate"
};


#define USAGE() \
	do { \
		puts(TEXT_USAGE); \
		exit(0); \
	} while (0)

#define NEXTARG() \
	do { \
		i++; \
		if (i >= argc) \
			exit(0); \
	} while (0)

//USAGE: imgcmp -c [-otheropts] img1 img2
//       imgcmp 
//       imgcmp [-d] [workdir] [outdir]
void ParseCmdLine(int argc, char *argv[]) {
	int i, j;

	for (i = 1; i != argc; i++) {
		if (*argv[i] != '-') {
			if (!workdir[0])
				strlcpy(workdir, argv[i], sizeof(workdir));
			else
				fprintf(stderr, "WARNING: ignored parameter '%s'\n", argv[i]);

			continue;
		}

		switch (argv[i][1]) {
			case 'a': //Add image
				// TODO: this is currently the default option
				break;
			case 'c': //Cache control
				NEXTARG();
				for (j = 0; j != ARRAYLEN(cache_cmd_strs) &&
					strcmp(argv[i], cache_cmd_strs[j]); j++);

				switch (j) {
					case CACHE_CMD_SETINDEX:
						NEXTARG();
						strlcpy(thumb_btree_fn, argv[i], sizeof(thumb_cache_fn));
						break;
					case CACHE_CMD_SETDATA:
						NEXTARG();
						strlcpy(thumb_cache_fn, argv[i], sizeof(thumb_cache_fn));
						break;
					case CACHE_CMD_DUMPALL:
						cache_dump = TC_DUMP_IMGS;
						break;
					case CACHE_CMD_DUMPINFO:
						cache_dump = TC_DUMP_INFO;
						break;
					case CACHE_CMD_DISABLE:
						cache_dont_use = 1;
						break;
					case CACHE_CMD_NOUPDATE:
						cache_no_update = 1;
						break;
					default:
						USAGE();
				}
				break;
			case 'd': //Deduplicate
				deduplicate_dir = 1;
				break;
			case 'h': //Help
			case '?':
				USAGE();
			case 'm': //coMpare <also takes method as option>
				switch (argv[i][2]) { //comparison method
					case 'a':
						comparison = IMG_CMP_ABS;
						break;
					case 'r':
						comparison = IMG_CMP_RANGE;
						break;
					case 'h': //histogram comparison type
						if (argv[i][3] == 'r')
							comparison = IMG_CMP_HISTRGB;
						else
							comparison = IMG_CMP_HISTHSV;
						break;
					case 'p':
						comparison = IMG_CMP_PHASH;
						break;
					default:
						fprintf(stderr, "WARNING: unrecognized comparison "
							"option '%c'\n", argv[i][2]);
				}
				NEXTARG();
				strlcpy(imgpath1, argv[i], sizeof(imgpath1));
				NEXTARG();
				strlcpy(imgpath2, argv[i], sizeof(imgpath2));
				break;
			case 'o': //Output filepath
				NEXTARG();
				strlcpy(outpath, argv[i], sizeof(outpath));
				break;
			case 'p': //percent difference of images (number of pixels)
				NEXTARG();
				npixels_diff = atoi(argv[i]);
				break;
			case 'r': //Recursive scan
				scan_recursive = 1;
				break;
			case 't': //pixel difference Tolerance
				NEXTARG();
				pixel_tolerance = atoi(argv[i]);
				break;
			case 'v': //verbose
				verbose++;
				break;
			case 'V': //version
				puts(TEXT_VERSION);
				exit(0);
			case '-': //full-string flags, for less commonly used options.
				
				break;
			default:
				fprintf(stderr, "WARNING: unrecognized option "
					"'%c', ignoring", argv[i][1]);
		}
	}
}


void ImageComparisonPerform(int method, const char *f1, const char *f2) {
	gdImagePtr img1 = NULL, img2 = NULL, imgresult = NULL;
	int x, y;
	int match, nunmatched;

	img1 = ThumbCreate(f1, NULL);
	img2 = ThumbCreate(f2, NULL);
	if (!img1 || !img2) {
		fprintf(stderr, "ERROR: failed to create thumbnail of image\n");
		goto end;
	}

	switch (method) {
		case IMG_CMP_ABS:
		case IMG_CMP_RANGE:
			imgresult = gdImageCreateTrueColor(THUMB_CX, THUMB_CY);
			if (!imgresult) {
				fprintf(stderr, "ERROR: failed to create result image\n");
				goto end;
			}
			if (method == IMG_CMP_ABS) {
				printf("Average color difference: 0x%08x\n",
					ImgGetAbsColorDiff(img1, img2, imgresult));
			} else {
				nunmatched = 0;
				for (y = 0; y != THUMB_CY; y++) {
					for (x = 0; x != THUMB_CX; x++) {
						match = ImgPixelCompareFuzzy(img1->tpixels[y][x], img2->tpixels[y][x]);
						if (match) {
							imgresult->tpixels[y][x] = 0xFFFFFF;
						} else {
							imgresult->tpixels[y][x] = 0x000000;
							nunmatched++;
						}
					}
				}
			}
			if (!ImgSavePng(outpath[0] ? outpath : "output.png", imgresult)) {
				fprintf(stderr, "ERROR: failed to save output image to file\n");
				goto end;
			}
			break;
		case IMG_CMP_HISTRGB:
		case IMG_CMP_HISTHSV:
			break;
		case IMG_CMP_PHASH:
			break;
		default:
			fprintf(stderr, "ERROR: unrecognized comparison method\n");
			goto end;
	}

end:
	if (img1)
		gdImageDestroy(img1);
	if (img2)
		gdImageDestroy(img2);
	if (imgresult)
		gdImageDestroy(imgresult);
}


#ifdef _WIN32

time_t FileTimeToUnixTime(FILETIME ft) {
	LARGE_INTEGER date, adjust;

	date.HighPart = ft.dwHighDateTime;
	date.LowPart  = ft.dwLowDateTime;

	adjust.QuadPart = 116444736000000000;
	date.QuadPart  -= adjust.QuadPart;

	return (time_t)(date.QuadPart / 10000000);
}


void UnixTimeToFileTime(time_t t, LPFILETIME pft) {
	LONGLONG ll;

	ll = Int32x32To64(t, 10000000) + 116444736000000000;
	pft->dwLowDateTime = (DWORD)ll;
	pft->dwHighDateTime = ll >> 32;
}

#endif


#ifdef _WIN32

time_t GetLastWriteTime(const char *filename) {
	WIN32_FILE_ATTRIBUTE_DATA fattribs;

	if (!GetFileAttributesEx(".", GetFileExInfoStandard, &fattribs)) {
		printerr("GetFileAttributesEx");
		return 0;
	}

	return FileTimeToUnixTime(fattribs.ftLastWriteTime);
}

#else

time_t GetLastWriteTime(const char *filename) {
	struct stat st;

	if (stat(".", &st) == -1) {
		perror("stat");
		return 0;
	}

	return st.st_mtime;
}

#endif


//strcpy is safe here because filename is always limited to 256 chars
int BuildPath(const char *filename) {
	char name[256], *next, *cur;

	strcpy(name, filename);
	cur = name;

#ifdef _WIN32
	if (isalpha(cur[0]) && cur[1] == ':' && cur[2] == PATH_SEPARATOR)
		cur += 3;
#else
	if (*cur == PATH_SEPARATOR)
		cur++;
#endif

	while ((next = strchr(cur, PATH_SEPARATOR))) {
		*next = '\0';

		if (createdir(name) == -1 && errno != EEXIST) {
			perror("mkdir");
			return 0;
		}

		*next = PATH_SEPARATOR;
		next++;
		cur = next;
	}

	return 1;
}


size_t strrplcpy(char *__restrict dst, const char *__restrict src,
				 size_t len, const char find, const char replacewith) {
	const char *s = src;

	while (len > 1 && *src) {
		if (*src != find)
			*dst = *src;
		else
			*dst = replacewith;

		src++;
		dst++;
		len--;
	}

	if (len)
		*dst = '\0';

	while (*src)
		src++;

	return src - s;
}


char *stpcpy(char *__restrict dst, const char *__restrict src) {
	while (*dst = *src) {
		dst++;
		src++;
	}
	return dst;
}


#if !(__BSD_VISIBLE)
#	ifndef USE_SSE_STRLCPY

size_t strlcpy(char *__restrict dst, const char *__restrict src, size_t len) {
	const char *s = src;

	while (len > 1 && *src) {
		*dst++ = *src++;
		len--;
	}

	if (len)
		*dst = '\0';

	while (*src)
		src++;

	return src - s;
}

#	else

size_t strlcpy(char *__restrict dst, const char *__restrict src, size_t len) {
	//xmm0 - data
	//xmm1 - comparison

#ifdef _WIN32
	__asm {
		mov edx, len
		and edx, edx
		jz totally_done
			mov esi, src
			mov edi, dst

			dec edx
			and edx, edx
			jz remainder_done

			mov ecx, esi
			and ecx, 0x0F
			jz do_sse

			mov eax, 16
			sub eax, ecx
			mov ecx, eax
			cmp ecx, edx
			jz remainder_loop

			align_loop:
				mov al, [esi]
				mov [edi], al

				inc esi
				inc edi		

				and al, al
				jz totally_done
				dec edx
				jz remainder_done
				dec ecx
			jnz align_loop

			cmp edx, 16
			jl remainder_loop
			
			do_sse:
			pxor xmm1, xmm1
			sse_loop:
				movdqa xmm0, [esi]

				pcmpeqb xmm1, xmm0
				pmovmskb eax, xmm1
				test eax, eax
				jnz sse_done

				movdqu [edi], xmm0

				add esi, 16
				add edi, 16

				sub edx, 16
				cmp edx, 16
				jge sse_loop
			sse_done:

			and edx, edx
			jz remainder_done

			remainder_loop:
				mov al, [esi]
				mov [edi], al

				inc esi
				inc edi

				and al, al
				jz totally_done
				dec edx
				jnz remainder_loop
			remainder_done:

			mov byte ptr [edi], 0
		totally_done:
	}
#else
	__asm__ (
		".intel_syntax noprefix\n"
		"mov edx, %0\n"
		"jz totally_done\n"
			"dec edx\n"

			"mov esi, %1\n"
			"mov edi, %2\n"

			"mov ecx, esi\n"
			"and ecx, 0x0F\n"
			"cmp ecx, edx\n"
			"jz remainder_loop\n"

			"align_loop:\n"
				"mov al, [esi]\n"
				"mov [edi], al\n"

				"inc esi\n"
				"inc edi\n"		

				"and al, al\n"
				"jz totally_done\n"
				"dec ecx\n"
				"jnz align_loop\n"
			"align_done:\n"

			"cmp edx, 16\n"
			"jl copy_remainder\n"

			"pxor xmm1, xmm1\n"
			"sse_loop:\n"
				"movdqu xmm0, [esi]\n"

				"pcmpeqb xmm1, xmm0\n"
				"pmovmskb eax, xmm1\n"
				"test eax, eax\n"
				"jz sse_done\n"

				"movdqu [edi], xmm0\n"

				"add esi, 16\n"
				"add edi, 16\n"

				"sub edx, 16\n"
				"cmp edx, 16\n"
				"jge sse_loop\n"
			"sse_done:\n"

			"remainder_loop:\n"
				"mov al, [esi]\n"
				"mov [edi], al\n"

				"inc esi\n"
				"inc edi\n"

				"and al, al\n"
				"jz totally_done\n"
				"dec edx\n"
				"jnz remainder_loop\n"
			"remainder_done:\n"

			"mov byte ptr [edi], 0\n"
		"totally_done:\n"
		".att_syntax"
		: : "m"(len), "m"(src), "m"(dest)
	)
#endif
	return 0; //idgaf for right now
}

#	endif
#endif

