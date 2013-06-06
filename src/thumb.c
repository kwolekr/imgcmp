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
 * thumb.c - 
 *    Contains routines pertaining to thumbnail cache
 */

#include "main.h"
#include "mmfile.h"
#include "bptree.h"
#include "img.h"
#include "hashtable.h"
#include "thumb.h"

LPHT cacheht;
LPBPTREE thumbbpt;
char thumb_btree_fn[256] = "thumbindex.db";
char thumb_cache_fn[256] = "thumbcache.db";
FMAPINFO cachemap;
int burstmode;
int nadded;


///////////////////////////////////////////////////////////////////////////////


int ThumbCacheBurstReadBegin(int reinit) {
	if (reinit) {
		if (!MMFileClose(&cachemap)) {
			fprintf(stderr, "ERROR: failed to close thumb cache\n");
			return 0;
		}
		burstmode = 0;
	}

	if (burstmode)
		return 1;

	if (!MMFileOpen(thumb_cache_fn,	THUMBCACHE_INITIAL_LEN, &cachemap)) {
		fprintf(stderr, "ERROR: failed to open thumb cache\n");
		return 0;
	}

	burstmode = 1;

	return 1;
}


int ThumbCacheBurstReadEnd() {
	if (burstmode) {
		if (!MMFileClose(&cachemap)) {
			fprintf(stderr, "ERROR: failed to close thumb cache\n");
			return 0;
		}
		burstmode = 0;
	}

	return 1;
}


gdImagePtr ThumbCreate(const char *filename, unsigned int *filesize) {
	gdImagePtr pic, im;

	pic = ImgLoadGd(filename, filesize);
	if (!pic)
		return NULL;

	im = gdImageCreateTrueColor(THUMB_CX, THUMB_CY);
	gdImageCopyResampled(im, pic, 0, 0, 0, 0, THUMB_CX, THUMB_CY, pic->sx, pic->sy);
	gdImageDestroy(pic);

	return im;
}


void _ThumbFlatten(int **tpixels, int mask) {
	int x, y;

	for (y = 0; y != THUMB_CY; y++) {
		for (x = 0; x != THUMB_CX; x++)
			tpixels[y][x] &= mask; //~0x00010101;
	}
}


int ThumbCacheAdd(FILE *tc, const char *filename, time_t mtime) {
	gdImagePtr thumb = NULL;
	unsigned int thumbsize, imgsize, offset;
	void *thumbdata;
	TCENTRY tcent;
	int status = 0, closetc = 0;

	if (!filename)
		return 0;

	if (!tc) {
		closetc = 1;
		tc = fopen(thumb_cache_fn, "rb+");
		if (!tc)
			goto end;

		if (fseek(tc, 0, SEEK_END))
			goto end;
	}

	thumb = ThumbCreate(filename, &imgsize);
	if (!thumb)
		goto end;

	thumbdata = gdImagePngPtr(thumb, (int *)&thumbsize);
	if (!thumbdata)
		goto end;

	tcent.mtime      = mtime;
	tcent.thumbfsize = thumbsize;
	tcent.thumbkey   = _ThumbCalcKey(thumb->tpixels);

	offset = _ThumbCacheWriteEntry(tc, &tcent, filename, thumbdata);
	if (!offset)
		goto end;

	status = _ThumbCacheUpdateStructures(filename, &tcent, offset, 0);

end:
	if (thumb)
		gdImageDestroy(thumb);
	if (thumbdata)
		gdFree(thumbdata);
	if (tc && closetc)
		fclose(tc);

	return status;
}


int ThumbCacheReplace(FILE *tc, const char *filename, LPTCRECORD ptcrec, time_t mtime) {
	void *thumbdata;
	gdImagePtr thumb = NULL;
	unsigned int origoffset, offset;
	uint32_t thumbsize;
	int status = 0, closetc = 0;

	if (!filename || !ptcrec)
		return 0;

	if (!thumbbpt) {
		thumbbpt = BptOpen(thumb_btree_fn);
		if (!thumbbpt)
			return 0;
	}

	if (BptRemove(thumbbpt, ptcrec->ent.thumbkey) <= 0)
		goto fail;

	thumb = ThumbCreate(filename, NULL);
	if (!thumb)
		goto fail;

	thumbdata = gdImagePngPtr(thumb, (int *)&thumbsize);
	if (!thumbdata)
		goto fail;

	if (!tc) {
		closetc = 1;
		tc = fopen(thumb_cache_fn, "rb+");
		if (!tc)
			goto fail;
	}

	origoffset = ftell(tc);
	if (thumbsize <= ptcrec->ent.thumbfsize) {
		if (fseek(tc, ptcrec->offset, SEEK_SET))
			goto fail;
	} else {
		if (fseek(tc, 0, SEEK_END))
			goto fail;
		ptcrec->offset = ftell(tc);
	}
	ptcrec->ent.mtime      = mtime;
	ptcrec->ent.thumbfsize = thumbsize;
	ptcrec->ent.thumbkey   = _ThumbCalcKey(thumb->tpixels);

	offset = _ThumbCacheWriteEntry(tc, &ptcrec->ent, filename, thumbdata);
	if (!offset)
		goto fail;

	status = _ThumbCacheUpdateStructures(filename, &ptcrec->ent, offset, 1);

fail:
	if (tc) {
		if (closetc)
			fclose(tc);
		else
			fseek(tc, origoffset, SEEK_SET);
	}
	if (thumb)
		gdImageDestroy(thumb);
	if (thumbdata)
		gdFree(thumbdata);
	return status;
}


int ThumbCacheRemove(unsigned int offset) {
	float thumbkey;
	char *fn, *filename;
	int status  = 0;
	FILE *tc    = NULL;
	char *fnbuf = NULL;

	if (burstmode) {
		LPTCENTRY ptcent;
	
		ptcent = (LPTCENTRY)((char *)cachemap.addr + offset);

		ptcent->mtime = TC_MTIME_DELETED;

		thumbkey = ptcent->thumbkey;
		filename = ptcent->filename;
	} else {
		TCENTRY entry;

		tc = fopen(thumb_cache_fn, "rb+");
		if (!tc)
			return 0;

		if (fseek(tc, offset, SEEK_SET) == -1)
			goto end;
		if (!fread(&entry, sizeof(TCENTRY), 1, tc))
			goto end;
		fnbuf = malloc(entry.fnlen + 1);
		if (!fread(fnbuf, entry.fnlen + 1, 1, tc))
			goto end;

		if (fseek(tc, offset, SEEK_SET) == -1)
			goto end;
		entry.mtime = TC_MTIME_DELETED;
		fwrite(&entry, sizeof(TCENTRY), 1, tc);

		thumbkey = entry.thumbkey;
		filename = fnbuf;
	}

	if (!thumbbpt) {
		thumbbpt = BptOpen(thumb_btree_fn);
		if (!thumbbpt)
			goto end;
	}

	if (!BptRemove(thumbbpt, thumbkey))
		goto end;

	if (cacheht) {
		fn = HtUnassociateItem(cacheht, filename);
		if (!fn)
			goto end;
		free(fn - sizeof(TCRECORD));
	}

	status = 1;
end:
	if (fnbuf)
		free(fnbuf);
	if (tc)
		fclose(tc);
	return status;
}


int ThumbCacheGet(int nitems, unsigned int *offsets,
				  LPTCENTRY *entries, gdImagePtr *thumbs) {
	unsigned char *thumbdata, *thumbbuf;
	LPTCENTRY ptcent;
	TCENTRY tcent;
	int i, nsuccess;
	FILE *file;

	if (!offsets || !entries || !thumbs)
		return 0;

	nsuccess = 0;

	if (burstmode) {
		for (i = 0; i != nitems; i++) {
			ptcent    = (LPTCENTRY)((char *)cachemap.addr + offsets[i]);
			thumbdata = (unsigned char *)ptcent + sizeof(TCENTRY) + ptcent->fnlen + 1;
			if (ptcent->thumbfsize >= THUMB_MAX_SIZE) {
				entries[i] = NULL;
				thumbs[i]  = NULL;
				fprintf(stderr, "WARNING: thumb filesize too big, ignoring\n");
				continue;
			}

			thumbs[i] = gdImageCreateFromPngPtr(ptcent->thumbfsize, thumbdata);
			if (!thumbs[i]) {
				entries[i] = NULL;		
				continue;
			}

			entries[i] = ptcent;
			nsuccess++;
		}
	} else {
		file = fopen(thumb_cache_fn, "rb");
		if (!file)
			return 0;

		nsuccess = 0;
		for (i = 0; i != nitems; i++) {
			thumbs[i]  = NULL;
			entries[i] = NULL;

			if (fseek(file, offsets[i], SEEK_SET))
				continue;

			fread(&tcent, sizeof(TCENTRY), 1, file);
			if (ferror(file))
				continue;

			if (tcent.thumbfsize >= THUMB_MAX_SIZE) {
				fprintf(stderr, "WARNING: thumb filesize too big, ignoring\n");
				continue;
			}
			
			ptcent = malloc(sizeof(TCENTRY) + tcent.fnlen + 1);
			memcpy(ptcent, &tcent, sizeof(TCENTRY));
			fread(ptcent->filename, 1, tcent.fnlen + 1, file);
			if (ferror(file)) {
				free(ptcent);			
				continue;
			}

			thumbbuf = malloc(tcent.thumbfsize);
			fread(thumbbuf, tcent.thumbfsize, 1, file);
			if (ferror(file)) {
				free(ptcent);
				free(thumbbuf);
				continue;
			}

			thumbs[i] = gdImageCreateFromPngPtr(tcent.thumbfsize, thumbbuf);
			free(thumbbuf);
			if (!thumbs[i]) {
				free(ptcent);		
				continue;
			}

			entries[i] = ptcent;
			nsuccess++;
		}
		fclose(file);
	}
	return nsuccess;
}


LPTCENTRY ThumbCacheLookup(unsigned int offset) {
	LPTCENTRY ptcent;
	TCENTRY entry;
	FILE *file;

	if (burstmode)
		return (LPTCENTRY)((char *)cachemap.addr + offset);

	ptcent = NULL;

	file = fopen(thumb_cache_fn, "rb");
	if (!file)
		goto end;

	if (fseek(file, offset, SEEK_SET))
		goto end;

	if (!fread(&entry, sizeof(TCENTRY), 1, file))
		goto end;

	ptcent = malloc(sizeof(TCENTRY) + entry.fnlen + 1);
	memcpy(ptcent, &entry, sizeof(TCENTRY));
	fread(ptcent->filename, entry.fnlen + 1, 1, file);
	if (ferror(file)) {
		free(ptcent);
		ptcent = NULL;
		goto end;
	}

end:
	if (file)
		fclose(file);
	return ptcent;
}


void ThumbCacheEnumerate(int level) {
	LPTCHEADER ptchdr;
	LPTCENTRY ptcent;
	unsigned char *thumbdata;
	unsigned int pos;
	gdImagePtr thumb;
	int nentries = 0, ndelentries = 0;

	if (!ThumbCacheBurstReadBegin(0)) {
		fprintf(stderr, "ERROR: failed to open cache for mapping\n");
		return;
	}

	if (level >= TC_DUMP_IMGS) {
		if (!outpath[0]) {
			fprintf(stderr, "ERROR: must specify an output path\n");
			return;
		}
		if (createdir(outpath) == -1 && errno != EEXIST) {
			perror("mkdir");
			return;
		}
		if (chdir(outpath) == -1) {
			perror("chdir");
			return;
		}
	}

	ptchdr = (LPTCHEADER)cachemap.addr;

	if (level >= TC_DUMP_INFO) {
		printf("Directory last modified: %s"
			"Thumb cache entries:\n"
			"file                      "
			"thumb key\tthumb len\tlast modified\n",
			asctime(localtime(&ptchdr->lastupdate)));
	}

	pos = sizeof(TCHEADER);
	while (pos < cachemap.maplen) {
		ptcent = (LPTCENTRY)((char *)cachemap.addr + pos);

		if (ptcent->mtime != TC_MTIME_DELETED) {
			if (level >= TC_DUMP_INFO) {
				printf("%-26s%f\t%d\t%s", ptcent->filename, ptcent->thumbkey,
					ptcent->thumbfsize, asctime(localtime(&ptcent->mtime)));
			}

			if (level >= TC_DUMP_IMGS) {
				thumbdata = (unsigned char *)ptcent + sizeof(TCENTRY) + ptcent->fnlen + 1;
				thumb = gdImageCreateFromPngPtr(ptcent->thumbfsize, thumbdata);
				if (!thumb) {
					fprintf(stderr, "ERROR: failed to create image from thumbcache\n");
					continue;
				}

				if (!ImgSavePng(ptcent->filename, thumb)) {
					if (errno == ENOENT) {
						if (verbose) 
							printf("creating directory structure for %s\n", ptcent->filename);
						if (!BuildPath(ptcent->filename)) {
							fprintf(stderr, "ERROR: failed to build "
								"directory to %s\n", ptcent->filename);
							gdImageDestroy(thumb);
							continue;
						}
						if (!ImgSavePng(ptcent->filename, thumb)) {
							fprintf(stderr, "ERROR: failed to save %s after "
								"building directory\n", ptcent->filename);
							gdImageDestroy(thumb);
							continue;
						}
					} else {
						fprintf(stderr, "ERROR: failed to save %s\n", ptcent->filename);
						gdImageDestroy(thumb);
						continue;
					}
				}
				gdImageDestroy(thumb);
			}
			nentries++;
		} else {
			ndelentries++;
		}
		pos += sizeof(TCENTRY) + ptcent->fnlen + 1 + ptcent->thumbfsize;
	}

	if (level >= TC_DUMP_IMGS && chdir(workdir) == -1) {
		perror("chdir");
		return;
	}

	printf("Number of thumb cache entries: %d\n"
		"Number of deleted thumb cache entries: %d\n",
		nentries, ndelentries);

	ThumbCacheBurstReadEnd();

}


/*
 * N.B.
 * When not in burst mode, the caller must free(dupfns[i] - sizeof(TCRECORD))
 */
int ThumbFindMatches(const char *filename, LPTCENTRY *dupents,
					 unsigned int *dupoffs, unsigned int nmaxdups) {
	int nitems, i, j, status, res;
	gdImagePtr img, *thumbs;
	unsigned int *offsets, dups;
	float key, delta;
	LPTCRECORD ptcrec;
	LPTCENTRY ptcent;
	KVPAIR *matches;
	LPTCENTRY *entries;
	char *fn = NULL;

	if (!filename || !dupents || !dupoffs)
		return -1;

	img     = NULL;
	matches = NULL;

	if (!thumbbpt) {
		thumbbpt = BptOpen(thumb_btree_fn);
		if (!thumbbpt)
			return -1;
	}
	
	status = -1;

	img = ThumbCreate(filename, NULL);
	if (!img) {
		fprintf(stderr, "ERROR: couldn't create thumbnail\n");
		goto end;
	}

	key = _ThumbCalcKey(img->tpixels);

	//(x + y)^2 - x^2 = 2xy + y^2
	delta = (6.f * (float)sqrt(key / 3.f) * DIFF_TOLERANCE) + (DIFF_TOLERANCE * DIFF_TOLERANCE);

	nitems = BptSearchRange(thumbbpt, key - delta, key + delta, &matches);
	if (nitems == BT_ERROR) {
		fprintf(stderr, "ERROR: tree lookup failure\n");
		goto end;
	}
	if (nitems == BT_NOTFOUND) {
		fprintf(stderr, "ERROR: no items found\n");
		goto end;
	}

	offsets = alloca(nitems * sizeof(unsigned int));
	thumbs  = alloca(nitems * sizeof(gdImagePtr));
	entries = alloca(nitems * sizeof(LPTCENTRY));

	j = 0;
	for (i = 0; i != nitems; i++) {
		if (matches[i].key == key) {
			if (cacheht) {
				if (!fn)
					fn = HtGetItem(cacheht, filename);
				if (fn) {
					ptcrec = (LPTCRECORD)(fn - sizeof(TCRECORD));
					if (matches[i].val == ptcrec->offset)
						continue;
				}
			} else {
				ptcent = ThumbCacheLookup(matches[i].val);
				if (!ptcent) {
					fprintf(stderr, "WARNING: tree contained invalid offset\n");
					continue;
				}
				res = strcmp(ptcent->filename, filename);
				if (!burstmode)
					free(ptcent);
				if (!res)
					continue;
			}
		}
		offsets[j] = matches[i].val;
		j++;
	} 

	if (!j)
		return 0;

	status = ThumbCacheGet(j, offsets, entries, thumbs);

	if (!status) {
		fprintf(stderr, "ERROR: failed to read thumbnails from cache\n");
		goto end;
	}

	dups = 0;
	for (i = 0; i != j; i++) {
		if (!thumbs[i]) {
			fprintf(stderr, "WARNING, couldn't get thumb\n");
			continue;
		}
		if (ImgCompareFuzzy(img, thumbs[i])) {
			if (dups >= nmaxdups) {
				fprintf(stderr, "WARNING: too many matches (>= %d), "
					"dropping others\n", nmaxdups);
				for (; i != j; i++) {
					if (burstmode)
						free(entries[i]);
					gdImageDestroy(thumbs[i]);
				}
				break;
			}
			dupents[dups] = entries[i];
			dupoffs[dups] = offsets[i];
			dups++;
		}
		gdImageDestroy(thumbs[i]); ///we might need to keep this on hand later for displaying, maybe?
	}

	status = dups;

end:
	if (matches)
		free(matches);
	if (img)
		gdImageDestroy(img);
	return status;
}


unsigned int _ThumbCacheWriteEntry(FILE *tc, LPTCENTRY ptcent,
						  const char *filename, void *thumbdata) {
	unsigned int fileoffset, len;
	unsigned char padding[ALIGN_BYTES];
	int padlen;

	len = strlen(filename);
	if (!len || len > UCHAR_MAX)
		return 0;

	padlen = (ALIGN_BYTES - ((ptcent->thumbfsize + len + 1) & ALIGN_MASK)) & ALIGN_MASK;
	memset(padding, 0, padlen);

	ptcent->thumbfsize += padlen;
	ptcent->fnlen       = (unsigned char)len;

	fileoffset = ftell(tc);
	fwrite(ptcent, sizeof(TCENTRY), 1, tc);
	fwrite(filename, 1, len + 1, tc);
	fwrite(thumbdata, ptcent->thumbfsize - padlen, 1, tc);
	fwrite(padding, 1, padlen, tc);
	if (ferror(tc))
		return 0;

	return fileoffset;
}


int _ThumbCacheUpdateStructures(const char *filename, LPTCENTRY ptcent,
								unsigned int offset, int update) {
	const char *fn;
	LPTCRECORD ptcrec;

	if (!thumbbpt) {
		thumbbpt = BptOpen(thumb_btree_fn);
		if (!thumbbpt)
			return 0;
	}

	if (!BptInsert(thumbbpt, ptcent->thumbkey, offset))
		return 0;

	if (cacheht) {
		if (!update) {
			ptcrec = malloc(sizeof(TCRECORD) + ptcent->fnlen + 1);
			ptcrec->offset = offset;
			memcpy(&ptcrec->ent, ptcent, sizeof(TCENTRY));
			memcpy(&ptcrec->ent.filename, filename, ptcent->fnlen + 1);

			HtInsertItem(cacheht, ptcrec->ent.filename, ptcrec->ent.filename);
		} else {
			fn = HtGetItem(cacheht, filename);
			if (!fn)
				return 0;

			ptcrec = (LPTCRECORD)(fn - sizeof(TCRECORD));
			ptcrec->offset = offset;
			memcpy(&ptcrec->ent, ptcent, sizeof(TCENTRY));
		}
	}

	return 1;
}


int ThumbCacheFlush() {
	if (thumbbpt)
		BptClose(thumbbpt);
	if (cacheht)
		HtResetContents(cacheht);

#ifdef _WIN32
	if (!DeleteFile(thumb_btree_fn)) {
		fprintf(stderr, "ERROR: failed to delete %s, err: %d\n",
			thumb_btree_fn, GetLastError());
	}
	if (!DeleteFile(thumb_cache_fn)) {
		fprintf(stderr, "ERROR: failed to delete %s, err: %d\n",
			thumb_cache_fn, GetLastError());
	}
#else
	if (remove(thumb_btree_fn) == -1)
		perror("remove thumb_btree_fn");
	if (remove(thumb_cache_fn) == -1)
		perror("remove thumb_cache_fn");
#endif
	return 1;
}


void _ThumbCacheBuildHt(FILE *tc) {
	LPTCRECORD ptcrec;
	TCENTRY entry;
	int reclen;

	if (cacheht)
		HtResetContents(cacheht);
	else
		cacheht = HtInit(4096, 0, HT_HASH_DEFAULT, 4);

	while (fread(&entry, sizeof(TCENTRY), 1, tc)) {
		if (entry.mtime != TC_MTIME_DELETED) {
			reclen = sizeof(TCRECORD) + entry.fnlen + 1;
			ptcrec = malloc(reclen);

			ptcrec->offset = ftell(tc) - sizeof(TCENTRY);
			ptcrec->ent    = entry;
			if (!fread(ptcrec->ent.filename, entry.fnlen + 1, 1, tc)) {
				fprintf(stderr, "ERROR: failed to read filename from thumb cache\n");
				return;
			}
			fseek(tc, entry.thumbfsize, SEEK_CUR);

			HtInsertItem(cacheht, ptcrec->ent.filename, ptcrec->ent.filename);
		}
	}
}


int ThumbCacheUpdate() {
	TCHEADER tch;
	int status = 0;
	time_t dirlastmod;
	FILE *tc;

	if (verbose)
		printf(" - Updating thumb cache\n");

	tc = fopen(thumb_cache_fn, "rb+");
	if (!tc) {
		if (errno == ENOENT) {
			tc = fopen(thumb_cache_fn, "wb+");
			if (!tc) {
				perror("fopen wb+");
				return 0;
			}

			tch.signature  = 'TMBC';
			tch.lastupdate = 0;
			fwrite(&tch, sizeof(TCHEADER), 1, tc);
			
			tc = freopen(thumb_cache_fn, "rb+", tc);
			if (!tc) {
				perror("freopen rb+");
				return 0;
			}
		} else {
			perror("fopen rb+");
			return 0;
		}
	}

	fread(&tch, sizeof(TCHEADER), 1, tc);

	if (tch.signature != 'TMBC') {
		fprintf(stderr, "ERROR: thumbcache signature does not match\n");
		goto done;
	}
	
	dirlastmod = GetLastWriteTime(".");
	if (tch.lastupdate >= dirlastmod) {
		if (tch.lastupdate > dirlastmod) {
			fprintf(stderr, "WARNING: thumbcache recorded last "
				"mtime > directory last mtime\n");
		}
		if (verbose)
			printf("Cache is up-to-date.\n");
		
		status = 1;
		goto done;
	}
	
	_ThumbCacheBuildHt(tc);

	if (fseek(tc, sizeof(TCHEADER) - sizeof(time_t), SEEK_SET) == -1) {
		perror("fseek");
		goto done;
	}
	fwrite(&dirlastmod, sizeof(time_t), 1, tc);

	_ThumbCacheUpdateDirScan(tc, "");

	printf("Added %d entries successfully.\n", nadded);

	status = 1;
done:
	if (tc)
		fclose(tc);
	return status;
}


void _ThumbCacheUpdateDirScan(FILE *tc, const char *dir) {
	unsigned int status;
	LPTCRECORD ptcrec;
	char *fn, relfn[MAX_PATH];
	int dirlen, len;
	time_t mtime;
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
				len = dirlen + strlen(fn);
				if (len + 1 >= MAX_PATH) {
					fprintf(stderr, "ERROR: total rel path len of "
						"%s too long, skipping\n", fn);
					continue;
				}

				strcpy(relfn + dirlen, fn);
				relfn[len]     = PATH_SEPARATOR;
				relfn[len + 1] = '\0';

				_ThumbCacheUpdateDirScan(tc, relfn);
			}
		} else if (ImgIsImageFile(fn)) {
			len = dirlen + strlen(fn);
			if (len >= MAX_PATH) {
				fprintf(stderr, "ERROR: total rel path len of "
					"%s too long, skipping\n", fn);
				continue;
			}

#ifdef _WIN32
			mtime = FileTimeToUnixTime(ffd.ftLastWriteTime);
#else
			mtime = st.st_mtime;
#endif
			strcpy(relfn + dirlen, fn);
			fn = HtGetItem(cacheht, relfn);
			if (fn) {
				ptcrec = (LPTCRECORD)(fn - sizeof(TCRECORD));
				if (mtime != ptcrec->ent.mtime) {
					if (verbose)
						printf("Updating %s...\n", relfn);
					if (!ThumbCacheReplace(tc, relfn, ptcrec, mtime))
						printerr("ThumbCacheReplace");
				}
			} else {
				if (verbose)
					printf("Adding %s to thumb cache...\n", relfn);
				if (!ThumbCacheAdd(tc, relfn, mtime))
					printerr("ThumbCacheAdd");
				else
					nadded++;
			}
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


float _ThumbCalcKey(int **tpixels) {
	unsigned int tr, tg, tb;
	float avg_r, avg_g, avg_b;
	int pixel, x, y;

	tr = 0;
	tg = 0;
	tb = 0;

	for (y = 0; y != THUMB_CY; y++) {
		for (x = 0; x != THUMB_CX; x++) {
			pixel = tpixels[y][x];
			tr += gdTrueColorGetRed(pixel);
			tg += gdTrueColorGetGreen(pixel);
			tb += gdTrueColorGetBlue(pixel);

		}
	}

	avg_r = (float)tr / (float)THUMB_NPIXELS;
	avg_g = (float)tg / (float)THUMB_NPIXELS;
	avg_b = (float)tb / (float)THUMB_NPIXELS;

	return (avg_r * avg_r) + (avg_g * avg_g) + (avg_b * avg_b);
}


#ifdef _WIN32

float _ThumbCalcKeyAsm(int **tpixels) {
	unsigned int tr, tg, tb;
	float avg_r, avg_g, avg_b;

	__asm {
		push ebx
		push esi
		push edi
		push ebp

		xor eax, eax
		xor esi, esi
		xor edi, edi

		mov ebp, tpixels

		push THUMB_CY //temporary stack allocations
		push ebp      //

		loopy:
			mov ebp, [esp]
			mov ecx, [esp + 4]
			mov ebp, [ebp + ecx * 4 - 4]

			mov ecx, THUMB_CX
			loopx:
				mov edx,  [ebp + ecx * 4 - 4]
				
				movzx ebx, dl
				add eax, ebx
				movzx ebx, dh
				add esi, ebx
				shr edx, 16
				movzx ebx, dl
				add edi, ebx

				dec ecx
			jne loopx

			dec dword ptr [esp + 4]
		jne loopy

		pop ebp
		pop ebp
		pop ebp

		mov tb, edi
		mov tg, esi
		mov tr, eax
		
		pop edi
		pop esi
		pop ebx
	}

	avg_r = (float)tr / (float)THUMB_NPIXELS;
	avg_g = (float)tg / (float)THUMB_NPIXELS;
	avg_b = (float)tb / (float)THUMB_NPIXELS;

	return (avg_r * avg_r) + (avg_g * avg_g) + (avg_b * avg_b);
}


float _ThumbCalcKeySSE(int **tpixels) {
	unsigned int tr, tg, tb;
	float avg_r, avg_g, avg_b;

	__asm {
		//xmm0 - blue accum
		//xmm1 - green accum
		//xmm2 - red accum
		//xmm3 - work reg
		//xmm4 - pixels
		//xmm5 - AND mask

		pxor xmm0, xmm0
		pxor xmm1, xmm1
		pxor xmm2, xmm2

		mov eax, 0xFF
		movd xmm5, eax
		pshufd xmm5, xmm5, 0

		mov esi, tpixels
		mov ecx, 0x4010
		loopy:
			mov edi, [esi]
			//prefetchnta [edi + 128]
			loopx:
				movdqu xmm4, [edi]
				
				movdqa xmm3, xmm4
				andps xmm3, xmm5
				paddd xmm0, xmm3

				movdqa xmm3, xmm4
				psrld xmm3, 8
				andps xmm3, xmm5
				paddd xmm1, xmm3

				movdqa xmm3, xmm4
				psrld xmm3, 16
				paddd xmm2, xmm3

				add edi, 16
				dec cl
			jnz loopx
			add esi, 4
			mov cl, 16
			dec ch
		jnz loopy

	}

	avg_r = (float)tr / (float)THUMB_NPIXELS;
	avg_g = (float)tg / (float)THUMB_NPIXELS;
	avg_b = (float)tb / (float)THUMB_NPIXELS;

	return (avg_r * avg_r) + (avg_g * avg_g) + (avg_b * avg_b);
}

#endif

