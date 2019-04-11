/* util.cpp Miscellaneous Utilities
 *
 * Copyright (c) 2019 by Adequate Systems, LLC.  All Rights Reserved.
 * See LICENSE.PDF   **** NO WARRANTY ****
 * https://github.com/mochimodev/mochimo/raw/master/LICENSE.PDF
 *
 * Date: 26 January 2019
 *
*/

#include "trigg_miner.h"

uint16_t get16(void *buff)
{
	return *((uint16_t *)buff);
}

void put16(void *buff, uint16_t val)
{
	*((uint16_t *)buff) = val;
}

uint32_t get32(void *buff)
{
	return *((uint32_t *)buff);
}

void put32(void *buff, uint32_t val)
{
	*((uint32_t *)buff) = val;
}

void put64(void *buff, void *val)
{
	((uint32_t *)buff)[0] = ((uint32_t *)val)[0];
	((uint32_t *)buff)[1] = ((uint32_t *)val)[1];
}

int cmp64(void *a, void *b)
{
	uint32_t *pa, *pb;

	pa = (uint32_t *)a;
	pb = (uint32_t *)b;
	if (pa[1] > pb[1]) return 1;
	if (pa[1] < pb[1]) return -1;
	if (pa[0] > pb[0]) return 1;
	if (pa[0] < pb[0]) return -1;
	return 0;
}

void shuffle32(uint32_t *list, uint32_t len)
{
	uint32_t *ptr, *p2, temp;

	if (len < 2) return;
	for (ptr = &list[len - 1]; len > 1; len--, ptr--) {
		p2 = &list[rand16() % len];
		temp = *ptr;
		*ptr = *p2;
		*p2 = temp;
	}
}

void fatal(char *fmt, ...)
{
	va_list argp;

	fprintf(stdout, "miner3: ");
	va_start(argp, fmt);
	vfprintf(stdout, fmt, argp);
	va_end(argp);
	printf("\n");
#ifdef _WINSOCKAPI_
	if (Needcleanup)
		WSACleanup();
#endif
	exit(2);
}

char *ntoa(uint8_t *a)
{
	static char s[24];

	sprintf(s, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
	return s;
}

int exists(char *fname)
{
	FILE *fp;

	fp = fopen(fname, "rb");
	if (!fp) return 0;
	fclose(fp);
	return 1;
}

char *bnum2hex(uint8_t *bnum)
{
	static char buff[20];

	sprintf(buff, "%02x%02x%02x%02x%02x%02x%02x%02x",
		bnum[7], bnum[6], bnum[5], bnum[4],
		bnum[3], bnum[2], bnum[1], bnum[0]);
	return buff;
}

int readtrailer(btrailer_t *trailer, char *fname)
{
	FILE *fp;

	fp = fopen(fname, "rb");
	if (fp == NULL) return VERROR;
	if (fseek(fp, -(sizeof(btrailer_t)), SEEK_END) != 0) {
	bad:
		fclose(fp);
		return VERROR;
	}
	if (fread(trailer, 1, sizeof(btrailer_t), fp) != sizeof(btrailer_t)) goto bad;
	fclose(fp);
	return VEOK;
}


int patch_addr(char *cblock, char *addrfile)
{
	FILE *fp, *fpout;
	uint8_t buff[TXADDRLEN];
	int ecode = 0;

	fp = fopen(addrfile, "rb");
	if (fp == NULL) {
		printf("\nTrace: Could not open addrfile.");
	}
	fpout = fopen(cblock, "r+b");
	if (fpout == NULL) {
		fclose(fp);
		printf("\nTrace: Unable to write updated candidate block.");
		return VERROR;
	}
	if (fread(buff, 1, TXADDRLEN, fp) != TXADDRLEN) ecode++;
	if (fseek(fpout, 4, SEEK_SET)) ecode++;
	if (fwrite(buff, 1, TXADDRLEN, fpout) != TXADDRLEN) ecode++;
	fclose(fpout);
	fclose(fp);
	return ecode;
}
