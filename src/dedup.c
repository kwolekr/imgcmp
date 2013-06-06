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
 * dedup.c - 
 *    Routines to deduplicate images in a directory.
 */

#include "main.h"
#include "hashtable.h"
#include "img.h"
#include "thumb.h"
#include "dedup.h"

//extern inline int ImgPixelCompareFuzzy(int p1, int p2);
LPHT ht_files_processed;


///////////////////////////////////////////////////////////////////////////////


void DedupPerform(const char *dir) {
	if (chdir(dir) == -1) {
		printerr("SetCurrentDirectory");
		return;
	}

	if (!ThumbCacheBurstReadBegin(0))
		return;

	ht_files_processed = HtInit(128, 0, HT_HASH_DEFAULT, 2);
	if (!ht_files_processed) {
		fprintf(stderr, "ERROR: failed to create ht\n");
		return;
	}

	if (verbose)
		printf(" - Deduplicating images in %s\n", dir);

	DedupDirScan("");

	ThumbCacheBurstReadEnd();
}


void DedupDirScan(const char *dir) {
	unsigned int status;
	LPTCENTRY pdupents[32];
	unsigned int dupoffs[ARRAYLEN(pdupents)];
	char *fn, relfn[MAX_PATH];
	int dirlen, len, nmatches, i;
#ifdef _WIN32
	HANDLE hFindFile;
	WIN32_FIND_DATA ffd;
#else
	DIR *dirp;
	struct stat st;
	struct dirent *entry;
#endif

	dirlen = strlen(dir);
	if (dirlen + 3 >= MAX_PATH) {
		fprintf(stderr, "ERROR: filename too long\n");
		return;
	}

	strcpy(relfn, dir);
#ifdef _WIN32
	strcpy(relfn + dirlen, "*");
	hFindFile = FindFirstFile(relfn, &ffd);
	if (hFindFile == INVALID_HANDLE_VALUE) {
		status = GetLastError();
		if (status != ERROR_FILE_NOT_FOUND)
			printerr("FindFirstFile");
		return;
	}

	do {
		fn = ffd.cFileName;
		if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && scan_recursive) {
#else
	dirp = opendir(relfn);
	if (!dirp) {
		perror("opendir");
		return;
	}

	while ((entry = readdir(dirp))) {
		if (lstat(entry->d_name, &st) == -1) {
			fprintf(stderr, "ERROR: couldn't stat %s, skipping\n", fn);
			continue;
		}

		fn = entry->d_name;
		if (S_ISDIR(st.st_mode) && scan_recursive) {
#endif
			if (!(fn[0] == '.' && (!fn[1] || (fn[1] == '.' && !fn[2])))) {
				if (!strcmp(fn, outpath)) //can't fit on the previous line
					continue;

				len = dirlen + strlen(fn);
				if (len + 1 >= MAX_PATH) {
					fprintf(stderr, "ERROR: total rel path len of "
						"%s too long, skipping\n", fn);
					continue;
				}

				strcpy(relfn + dirlen, fn);
				relfn[len]     = PATH_SEPARATOR;
				relfn[len + 1] = '\0';

				DedupDirScan(relfn);
			}
		} else if (ImgIsImageFile(fn) && !HtGetItem(ht_files_processed, fn)) {
			len = dirlen + strlen(fn);
			if (len >= MAX_PATH) {
				fprintf(stderr, "ERROR: total rel path "
					"%s too long, skipping\n", fn);
				continue;
			}

			printf("checking %s...\n", fn);
			strcpy(relfn + dirlen, fn);

			nmatches = ThumbFindMatches(relfn, pdupents, dupoffs, ARRAYLEN(pdupents));
			if (nmatches == -1) {
				printerr("ThumbFindMatches");
				continue;
			}
			for (i = 0; i != nmatches; i++) {
				printf("duplicate of %s found, %s\n", relfn, pdupents[i]->filename);
				DedupHandleDuplicate(relfn, pdupents[i]->filename, dupoffs[i]);
				HtInsertItem(ht_files_processed, pdupents[i]->filename, pdupents[i]->filename);
			}
			//if (nmatches && move_original)
			//	DedupHandleDuplicate(relfn, pdupents[i]->filename, dupoffs[i]);
		}
#ifdef _WIN32
	} while (FindNextFile(hFindFile, &ffd));
	
	FindClose(hFindFile);
#else
	}

	if (closedir(dirp) == -1)
		perror("closedir");
#endif
}


void DedupHandleDuplicate(const char *cmpfn, const char *dupfn,
						  unsigned int dupoffset) {
	char fname[256];
	int dirlen;

	if (!outpath[0]) {
		outpath[0] = '.';
		outpath[1] = '\0';
	}

	dirlen = snprintf(fname, sizeof(fname), "%s%cdup-%08x%c", 
		outpath, PATH_SEPARATOR,
		HtDefaultHash(cmpfn, strlen(cmpfn)), PATH_SEPARATOR);
	if (dirlen < 0) {
		perror("snprintf: too long");
		return;
	}
	if (strrplcpy(fname + dirlen, dupfn, sizeof(fname) - dirlen, 
		PATH_SEPARATOR, '_') >= sizeof(fname) - dirlen) {
		fprintf(stderr, "ERROR: output path was too long\n");
		return;
	}

	if (rename(dupfn, fname)) {
		if (errno != ENOENT) {
			perror("rename");
			return;
		}
		if (!BuildPath(fname)) {
			fprintf(stderr, "ERROR: failed to build path\n");
			return;
		}
		if (rename(dupfn, fname)) {
			perror("rename: failed after building path");
			return;
		}
	}
	if (!ThumbCacheRemove(dupoffset)) {
		fprintf(stderr, "ERROR: failed to remove thumb from cache\n");
		return;
	}
}

