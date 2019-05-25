/* util.c Miscellaneous Utilities
 *
 * Copyright (c) 2019 by Adequate Systems, LLC.  All Rights Reserved.
 * See LICENSE.PDF   **** NO WARRANTY ****
 * https://github.com/mochimodev/mochimo/raw/master/LICENSE.PDF
 *
 * Date: 26 January 2019
 *
*/

#include "trigg_util.h"

#ifdef __cplusplus
extern "C" {
#endif 

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

char *ntoa(uint8_t *a)
{
	static char s[24];

	sprintf(s, "%d.%d.%d.%d", a[0], a[1], a[2], a[3]);
	return s;
}

int nonblock(SOCKET sd)
{
    unsigned long arg = 1L;
    return ioctl(sd, FIONBIO, (unsigned long FAR *) &arg);
}

char *bnum2hex(uint8_t *bnum)
{
	static char buff[20];

	sprintf(buff, "%02x%02x%02x%02x%02x%02x%02x%02x",
		bnum[7], bnum[6], bnum[5], bnum[4],
		bnum[3], bnum[2], bnum[1], bnum[0]);
	return buff;
}

// covert string(domain or IPv4 numbers-and-dots notation) to binary ip
uint32_t str2ip(char *addrstr)
{
    if (addrstr == NULL) 
        return 0;

    struct hostent *host;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    if (addrstr[0] < '0' || addrstr[0] > '9') {
        if ((host = gethostbyname(addrstr)) == NULL)
            return 0;
        else
            memcpy((char *)&(addr.sin_addr.s_addr), host->h_addr_list[0], host->h_length);
    } else {
        addr.sin_addr.s_addr = inet_addr(addrstr);
    }
    return addr.sin_addr.s_addr;
}

void trigg_coreip_shuffle(uint32_t *list, uint32_t len)
{
    uint32_t *ptr, *p2, temp, ix;

    if (len < 2) 
        return;
    for (ptr = &list[len - 1]; len > 1; len--, ptr--) {
        ix = rand16() % len;
        p2 = &list[ix];
        temp = *ptr;
        *ptr = *p2;
        *p2 = temp;
    }
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

#ifdef __cplusplus
}
#endif 

