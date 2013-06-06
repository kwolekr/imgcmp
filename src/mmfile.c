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
 * mmfile.c - 
 *    Routines to open, resize, and close memory-mapped files.
 */

#include "main.h"
#include "mmfile.h"


///////////////////////////////////////////////////////////////////////////////


#ifdef _WIN32

int MMFileOpen(const char *filename, unsigned int createlen, LPFMAPINFO fmi) {
	unsigned int maplen;
	int status;
	
	if (!fmi)
		return 0;

	status = 1;

	if (filename) {
		fmi->hFile = CreateFile(filename, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, 
			createlen ? OPEN_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_HIDDEN, NULL);
		if (fmi->hFile == INVALID_HANDLE_VALUE) {
			printerr("CreateFile");
			return 0;
		}

		maplen = GetFileSize(fmi->hFile, NULL);
		if (maplen < createlen) {
			maplen = createlen;
			status = -1;
			printf("new file %s, growing to %d bytes\n", filename, maplen);
		}
	} else {
		if (!createlen)
			return 0;

		fmi->hFile = INVALID_HANDLE_VALUE;
		maplen = createlen;
		status = -1;
	}

	fmi->hMap = CreateFileMapping(fmi->hFile, NULL, PAGE_READWRITE, 0, maplen, NULL);
	if (!fmi->hMap) {
		printerr("CreateFileMapping");
		goto fail_file;
	}

	fmi->addr = MapViewOfFile(fmi->hMap, FILE_MAP_ALL_ACCESS, 0, 0, maplen);
	if (!fmi->addr) {
		printerr("MapViewOfFile");
		goto fail;
	}

	fmi->maplen = maplen;
	
	return status;
fail:
	CloseHandle(fmi->hMap);
	fmi->hMap = 0;
fail_file:
	CloseHandle(fmi->hFile);
	fmi->hFile = 0;
	return 0;
}


int MMFileResize(LPFMAPINFO fmi, unsigned int newlen) {
	if (!fmi)
		return 0;

	if (!UnmapViewOfFile(fmi->addr)) {
		printerr("UnmapViewOfFile");
		return 0;
	}

	CloseHandle(fmi->hMap);

	fmi->hMap = CreateFileMapping(fmi->hFile, NULL, PAGE_READWRITE, 0, newlen, NULL);
	if (!fmi->hMap) {
		printf("%d", GetLastError());
		printerr("CreateFileMapping");
		return 0;
	}

	fmi->addr = MapViewOfFile(fmi->hMap, FILE_MAP_ALL_ACCESS, 0, 0, newlen);
	if (!fmi->addr) {
		printerr("MapViewOfFile");
		return 0;
	}

	fmi->maplen = newlen;

	return 1;
}


int MMFileClose(LPFMAPINFO fmi) {
	if (!fmi)
		return 0;

	if (!UnmapViewOfFile(fmi->addr)) {
		printerr("UnmapViewOfFile");
		return 0;
	}

	CloseHandle(fmi->hMap);
	CloseHandle(fmi->hFile);
	fmi->hMap  = 0;
	fmi->hFile = 0;

	fmi->addr   = NULL;
	fmi->maplen = 0;

	return 1;
}

#else

int MMFileOpen(const char *filename, unsigned int createlen, LPFMAPINFO fmi) {
	int maplen, status;
	struct stat st;

	if (!fmi)
		return 0;

	status = 1;

	if (filename) {
		fmi->fd = open(filename, O_RDWR | (createlen ? O_CREAT : 0), 0755);
		if (fmi->fd == -1) {
			perror("open");
			return 0;
		}

		if (fstat(fmi->fd, &st) == -1) {
			perror("fstat");
			goto fail;
		}

		maplen = st.st_size;
		if (maplen < createlen) {
			maplen = createlen;
			status = -1;
			printf("new file %s, growing to %d bytes\n", filename, maplen);
			if (ftruncate(fmi->fd, maplen) == -1) {
				perror("ftruncate");
				goto fail;
			}
		}
	} else {
		if (!createlen)
			return 0;

		fmi->fd = -1;
		maplen  = createlen;
		status  = -1;
	}

	fmi->addr = mmap(NULL, maplen, PROT_READ | PROT_WRITE,
		MAP_SHARED | (fmi->fd == -1 ? MAP_ANON : 0), fmi->fd, 0);
	if (fmi->addr == MAP_FAILED) {
		perror("mmap");
		goto fail;
	}

	fmi->maplen = maplen;
	
	return status;
fail:
	close(fmi->fd);
	fmi->fd = -1;
	return 0;
}


int MMFileResize(LPFMAPINFO fmi, unsigned int newlen) {
	if (!fmi)
		return 0;

	if (munmap(fmi->addr, fmi->maplen) == -1) {
		perror("munmap");
		return 0;
	}

	if (fmi->fd != -1 && ftruncate(fmi->fd, newlen) == -1) {
		perror("ftruncate");
		return 0;
	}

	fmi->addr = mmap(NULL, newlen, PROT_READ | PROT_WRITE,
		MAP_SHARED | (fmi->fd == -1 ? MAP_ANON : 0), fmi->fd, 0);
	if (fmi->addr == MAP_FAILED) {
		perror("mmap");
		return 0;
	}

	fmi->maplen = newlen;

	return 1;
}

int MMFileClose(LPFMAPINFO fmi) {
	if (!fmi)
		return 0;

	if (munmap(fmi->addr, fmi->maplen) == -1) {
		perror("munmap");
		return 0;
	}
	
	if (close(fmi->fd) == -1)
		perror("close");

	fmi->fd     = -1;
	fmi->addr   = NULL;
	fmi->maplen = 0;

	return 1;
}

#endif

