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

#ifndef MMFILE_HEADER
#define MMFILE_HEADER

typedef struct _fmapinfo {
	#ifdef _WIN32
		HANDLE hFile;
		HANDLE hMap;
	#else
		int fd;
	#endif
	unsigned int maplen;
	void *addr;
} FMAPINFO, *LPFMAPINFO;

int MMFileOpen(const char *filename, unsigned int createlen, LPFMAPINFO fmi);
int MMFileResize(LPFMAPINFO fmi, unsigned int newlen);
int MMFileClose(LPFMAPINFO fmi);


#ifdef _WIN32

static inline int MMFileFlush(LPFMAPINFO fmi, unsigned int flushlen) {
	if (!FlushViewOfFile(fmi->addr, flushlen)) {
		printerr("FlushViewOfFile");
		return 0;
	}
	
	return 1;
}

#else

static inline int MMFileFlush(LPFMAPINFO fmi, unsigned int flushlen) {
	if (msync(fmi->addr, flushlen, MS_SYNC) == -1) {
		perror("msync");
		return 0;
	}
	
	return 1;
}

#endif

#endif //MMFILE_HEADER

