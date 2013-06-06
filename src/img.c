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
 * img.c - 
 *    Routines to load, save, compare and verify images using libgd.
 */

#include "main.h"
#include "img.h"

inline int ImgPixelCompareFuzzy(int p1, int p2);


///////////////////////////////////////////////////////////////////////////////


gdImagePtr ImgLoadGd(const char *filename, unsigned int *filesize) {
	FILE *file;
	gdImagePtr img;
	char blah[4], *tmp;
	uint16_t sig16;
	uint32_t sig32;
	long filelen;

	if (!filename)
		return NULL;

	file = NULL;
	tmp  = NULL;
	img  = NULL;

	file = fopen(filename, "rb");
	if (!file)
		return NULL;

	if (!fread(blah, 4, 1, file))
		goto done;

	fseek(file, 0, SEEK_END);
	filelen = ftell(file);
	rewind(file);

	if (filesize)
		*filesize = filelen;

	tmp = malloc(filelen);
	if (!fread(tmp, filelen, 1, file))
		goto done;

	sig16 = LE16(*(uint16_t *)blah);
	sig32 = LE32(*(uint32_t *)blah);

	if (sig16 == 0xD8FF) {
		img = gdImageCreateFromJpegPtr(filelen, tmp);
	} else if (sig32 == 'GNP\x89') {
		img = gdImageCreateFromPngPtr(filelen, tmp);
	} else if (sig32 == '8FIG') {
		img = gdImageCreateFromGifPtr(filelen, tmp);
	} else if (sig16 == 'MB') {
		img = gdImageCreateFromWBMPPtr(filelen, tmp);
	} else {
		fprintf(stderr, "WARNING: %s is an unsupported image format\n", filename);
	}

done:
	if (file)
		fclose(file);
	if (tmp)
		free(tmp);
	return img;
}


int ImgSavePng(const char *filename, gdImagePtr im) {
	FILE *out;
	int size;
	char *data;

	out = fopen(filename, "wb");
	if (!out)
		return 0;

	data = (char *)gdImagePngPtr(im, &size);
	if (!data) {
		fclose(out);
		return 0;
	}

	fwrite(data, 1, size, out);

	fclose(out);
	gdFree(data);
	return 1;
}


int ImgIsImageFile(const char *filename) {
	const char *ext;

	ext = strrchr(filename, '.');
	if (!ext)
		return 0;

	switch (LE32(UAR32(ext))) {
		case 'gpj.': //.jpg
		case 'gnp.': //.png
		case 'fig.': //.gif
		case 'epj.': //.jpeg
		case 'pmb.': //.bmp
		case 'bid.': //.dib
			return 1;
	}

	return 0;
}


int ImgCompareFuzzy(gdImagePtr img1, gdImagePtr img2) {
	int x, y, sx, sy, npixwrong, match;
	float aspect_diff;
	gdImagePtr imgtmp;
	
	match  = 1;
	imgtmp = NULL;

	if (img1->sx != img2->sx || img1->sy != img2->sy) {
		aspect_diff = ((float)img1->sy / (float)img1->sx) - ((float)img2->sy / (float)img2->sx);
		if (aspect_diff >= MAX_RATIODIFF || aspect_diff <= -MAX_RATIODIFF)
			return 0;

		if (img1->sx * img1->sy < img2->sx * img2->sy) {
			sx = img1->sx;
			sy = img1->sy;
			imgtmp = gdImageCreateTrueColor(sx, sy);
			gdImageCopyResampled(imgtmp, img2, 0, 0, 0, 0, sx, sy, img2->sx, img2->sy);
			img2 = imgtmp;
		} else {
			sx = img2->sx;
			sy = img2->sy;
			imgtmp = gdImageCreateTrueColor(sx, sy);
			gdImageCopyResampled(imgtmp, img1, 0, 0, 0, 0, sx, sy, img1->sx, img1->sy);
			img1 = imgtmp;
		}
	} else {
		sx = img1->sx;
		sy = img1->sy;
	}

	npixwrong = 0;
	for (y = 0; y != sy; y++) {
		for (x = 0; x != sx; x++) {
			if (!ImgPixelCompareFuzzy(img1->tpixels[y][x], img2->tpixels[y][x])) {
				npixwrong++;
				if (npixwrong >= MAX_PIXELDIFF) {
					match = 0;
					break;
				}
			}
		}
	}

	if (imgtmp)
		gdImageDestroy(imgtmp);
	return match;
}


int ImgCompareExact(gdImagePtr img1, gdImagePtr img2) {
	int y, sx, sy;

	sx = img1->sx;
	sy = img1->sy;
	if (sx != img2->sx || sy != img2->sy)
		return 0;

	for (y = 0; y != sy; y++) {
		if (memcmp(img1->tpixels[y], img2->tpixels[y], sx * sizeof(img1->tpixels[y][0])))
			return 0;
	}

	return 1;
}


int ImgGetAbsColorDiff(gdImagePtr img1, gdImagePtr img2, gdImagePtr imgresult) {
	int p1, p2;
	int x, y;
	int dr, dg, db;
	int tr, tg, tb;

	tr = tg = tb = 0;

	for (y = 0; y != THUMB_CY; y++) {
		for (x = 0; x != THUMB_CX; x++) {
			p1 = img1->tpixels[y][x];
			p2 = img2->tpixels[y][x];
			
			dr = abs(gdTrueColorGetRed(p1)   - gdTrueColorGetRed(p2));
			dg = abs(gdTrueColorGetGreen(p1) - gdTrueColorGetGreen(p2));
			db = abs(gdTrueColorGetBlue(p1)  - gdTrueColorGetBlue(p2));
			
			tr += dr;
			tg += dg;
			tb += db;

			imgresult->tpixels[y][x] = (dr << 16) | (dg << 8) | (db << 0);
		}
	}
	tr /= THUMB_NPIXELS;
	tg /= THUMB_NPIXELS;
	tb /= THUMB_NPIXELS;

	return (tr << 16) | (tg << 8) | (tb << 0);
}


#if 0
#define floor2(x) ((int)(x))

void gdImageCopyResampled(gdImagePtr dst, gdImagePtr src,
						  int dstX, int dstY, int srcX, int srcY, 
						  int dstW, int dstH, int srcW, int srcH) {
	int x, y;
	double sy1, sy2, sx1, sx2;

	for (y = dstY; y < dstY + dstH; y++) {
		sy1 = ((double)y       - (double)dstY) * (double)srcH / (double)dstH;
		sy2 = ((double)(y + 1) - (double)dstY) * (double)srcH / (double)dstH;

		for (x = dstX; x < dstX + dstW; x++) {
			double sx, sy;
			double spixels = 0;
			double red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;

			sx1 = ((double)x       - (double)dstX) * (double)srcW / (double)dstW;
			sx2 = ((double)(x + 1) - (double)dstX) * (double)srcW / (double)dstW;

			sy = sy1;
			do {
				double yportion;

				if (floor2(sy) == floor2(sy1)) {
					yportion = 1.0 - (sy - floor2(sy));
					if (yportion > sy2 - sy1)
						yportion = sy2 - sy1;
					sy = floor2(sy);
				} else if (sy == floor2(sy2)) {
					yportion = sy2 - floor2(sy2);
				} else {
					yportion = 1.0;
				}

				sx = sx1;
				do {
					double xportion;
					double pcontribution;
					int p;

					if (floor2(sx) == floor2(sx1)) {
						xportion = 1.0 - (sx - floor2(sx));
						if (xportion > sx2 - sx1)
							xportion = sx2 - sx1;
						sx = floor2 (sx);
					} else if (sx == floor2(sx2)) {
						xportion = sx2 - floor2(sx2);
					} else {
						xportion = 1.0;
					}
					pcontribution = xportion * yportion;

					p = gdImageGetTrueColorPixel(src, (int)sx + srcX, (int)sy + srcY);

					red   += gdTrueColorGetRed (p) * pcontribution;
					green += gdTrueColorGetGreen (p) * pcontribution;
					blue  += gdTrueColorGetBlue (p) * pcontribution;
					spixels += xportion * yportion;

					sx += 1.0;
				} while (sx < sx2);
				sy += 1.0;
			} while (sy < sy2);

			if (spixels != 0.0) {
				red   /= spixels;
				green /= spixels;
				blue  /= spixels;
			}

			if (red > 255.0)
				red = 255.0;
			if (green > 255.0)
				green = 255.0;
			if (blue > 255.0)
				blue = 255.0;

			gdImageSetPixel(dst, x, y, gdTrueColorAlpha((int)red, (int)green, (int)blue, 0));
		}
	}
}
#endif

