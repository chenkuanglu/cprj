#include "miner.h"

#include "fpga/uart_fpga.h"
#include "fpga/hash_fpga.h"
#include "../fpga/hash_fpga.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#include <memory.h>
//#include <mm_malloc.h>

#include "sha3/sph_blake.h"

#include "lyra2/Lyra2.h"

#define CS0   (0x243F6A88)
#define CS1   (0x85A308D3)
#define CS2   (0x13198A2E)
#define CS3   (0x03707344)
#define CS4   (0xA4093822)
#define CS5   (0x299F31D0)
#define CS6   (0x082EFA98)
#define CS7   (0xEC4E6C89)

void trigg_prehash(void *output, const void *input)
{
	uint32_t hash[8+8];
	sph_blake256_context ctx_blake;

	sph_blake256_init(&ctx_blake);
	sph_blake256(&ctx_blake, input, 80);

    int n = 0;
    int bit_len = ((unsigned)ctx_blake.ptr << 3) + n;
    if (ctx_blake.ptr == 0 && n == 0) {
        ctx_blake.T0 = SPH_C32(0xFFFFFE00);
        ctx_blake.T1 = SPH_C32(0xFFFFFFFF);
    } else if (ctx_blake.T0 == 0) {
        ctx_blake.T0 = SPH_C32(0xFFFFFE00) + bit_len;
        ctx_blake.T1 = SPH_T32(ctx_blake.T1 - 1);
    } else {
        ctx_blake.T0 -= 512 - bit_len;
    }
    ctx_blake.T0 += 512;
    if (ctx_blake.T0 < 512) {
        ctx_blake.T1 += 1;
    }

	hash[0]  = ctx_blake.H[0];
	hash[1]  = ctx_blake.H[1];
	hash[2]  = ctx_blake.H[2];
	hash[3]  = ctx_blake.H[3];
	hash[4]  = ctx_blake.H[4];
	hash[5]  = ctx_blake.H[5];
	hash[6]  = ctx_blake.H[6];
	hash[7]  = ctx_blake.H[7];
	
	hash[8]  = ctx_blake.S[0] ^ CS0;
	hash[9]  = ctx_blake.S[1] ^ CS1;
	hash[10] = ctx_blake.S[2] ^ CS2;
	hash[11] = ctx_blake.S[3] ^ CS3;
	hash[12] = ctx_blake.T0   ^ CS4;
	hash[13] = ctx_blake.T0   ^ CS5;
	hash[14] = ctx_blake.T1   ^ CS6;
	hash[15] = ctx_blake.T1   ^ CS7;

	memcpy(output, hash, 32+32);
}

void trigghash(void *state, const void *input)
{
	sph_blake256_context ctx_blake;
	uint32_t _ALIGN(64) hash[16];
	uint64_t* lyra2z_matrix;

	const int i = BLOCK_LEN_INT64 * 8 * 8 * 8;
    lyra2z_matrix = malloc( i );

	sph_blake256_init(&ctx_blake);
	sph_blake256(&ctx_blake, input, 80);
	sph_blake256_close(&ctx_blake, hash);

	LYRA2Z( lyra2z_matrix, hash, 32, hash, 32, hash, 32, 8, 8, 8 );

	memcpy(state, hash, 32);
	free(lyra2z_matrix);
}

int fpga_scanhash_trigg(int id, const struct chip_config* chip_config,  uint32_t max_nonce, struct  miner_task* miner_task)
{
    #define NONCE_OFT32		19	
    
    #define __PRINT_ENDIAN_DATA 0
	#define __PRINT_FIRST_HASH  0 
	
	#define __FIXED_ENDIAN_DATA 0

	uint32_t prehash_iv[8+8];         // 16 word
	uint32_t msg_pading[4+12];      // 16 word
	int endingmsg_len = 4*4;
	
	uint32_t hash32[8+8];

	uint32_t *pdata = miner_task->work.data;
	uint32_t *ptarget = miner_task->work.target;

	const uint32_t Htarg = ptarget[7];
	const uint32_t first_nonce = pdata[NONCE_OFT32];
	pdata[NONCE_OFT32] = Htarg;
	uint32_t* endiandata = miner_task->lyra2z_ctx.endiandata;

	/*save first_nonce/max_nonce */
	miner_task->start_nonce = first_nonce;
	miner_task->end_nonce = max_nonce;

	applog(LOG_INFO, "chip %d: 1st_nonce = %08x, max_nonce = %08x, target=%08x", id, first_nonce, max_nonce, Htarg);
	// we need bigendian data...

#if __PRINT_ENDIAN_DATA > 0
    char endiandata_str[1024];
    applog(LOG_BLUE, "");
#endif

	for (int i=0; i < 20; i++) {
		be32enc(&endiandata[i], pdata[i]);
#if __PRINT_ENDIAN_DATA > 0
        bin2hex(endiandata_str, (unsigned char *)(endiandata + i), 4);
        applog(LOG_BLUE, "endiandata[%02d]:  %s", i, endiandata_str);
#endif		
	}

#if __FIXED_ENDIAN_DATA > 0
    char fixed_str[1024] = "000000203dcdc33a1b98b05052d2e2ac612dfa7c0c2ea7e8ee188674002df31545f49f0f230bc53a6d043fd92134084cef2951d5c3ac2cdd6fa479aabc756a82da4ae4fca8b2c15a024f021b00000080";

    hex2bin((unsigned char *)endiandata, fixed_str, 80);
    bin2hex(fixed_str, (unsigned char *)endiandata, 80);
    applog(LOG_BLUE, "");
    applog(LOG_BLUE, "fixed endiandata: %s", fixed_str);
#endif 	

#if __PRINT_ENDIAN_DATA > 0
    applog(LOG_BLUE, "");
	bin2hex(endiandata_str, (unsigned char *)endiandata, 80);
    applog(LOG_BLUE, "endiandata: %s", endiandata_str);
#endif

	trigg_prehash(hash32, endiandata);

	if (opt_asic_mode)
	{
	    // prehash_iv
    	memset(prehash_iv, 0, sizeof(prehash_iv));
        memcpy(prehash_iv, hash32, sizeof(hash32));
        // msg_pading
        memset(msg_pading, 0, sizeof(msg_pading));
        memcpy(msg_pading, endiandata+16, endingmsg_len);
        msg_pading[4] = 0x80L;
        msg_pading[13] = 0x01000000L;
        msg_pading[14] = 0x0L;
        msg_pading[15] = 0x80020000L;
	}

	struct constant constant;
	if (opt_asic_mode)
	{
	    constant.start_nonce	= first_nonce;
    	constant.end_nonce	    = max_nonce;

    	constant.prehash.p	    = prehash_iv;
    	constant.prehash.len	= 16 * sizeof(uint32_t);

    	constant.ending_msg.p   = msg_pading;
    	constant.ending_msg.len = 16 * sizeof(uint32_t);
	}
	else
	{
    	constant.start_nonce	= first_nonce;
    	constant.end_nonce	    = max_nonce;

    	constant.prehash.p	    = hash32;
    	constant.prehash.len	= 8 * sizeof(uint32_t);

    	constant.ending_msg.p   = (void *)(endiandata + 16);
    	constant.ending_msg.len = 4 * sizeof(uint32_t);
	}

	fpga_post_constant(chip_config->fd_uart, chip_config->addr, &constant);

#if __PRINT_FIRST_HASH > 0
    char hash_str[128];   
    
    be32enc(&endiandata[NONCE_OFT32], first_nonce);
    trigghash(hash32, endiandata);
    
    for (int i = 0; i < 8; i++)
    {
        be32enc(&hash32[i], hash32[i]);
        bin2hex(hash_str, (unsigned char *)(hash32 + i), 4);
        applog(LOG_BLUE, "first hash[%d]:   %s", i, hash_str);
    }   
#endif	

	return 0;
}

int fulltest_hash_trigg(int id, uint32_t hitnonce, struct  miner_task* miner_task)
{
	int ret = 0;
	uint32_t* endiandata = miner_task->lyra2z_ctx.endiandata;
	uint32_t* ptarget   = miner_task->work.target;
	uint32_t Htarg = ptarget[7];
	uint32_t hash32[8];

	be32enc(&endiandata[NONCE_OFT32], hitnonce);

	trigghash(hash32, endiandata);

	if(fulltest(hash32, ptarget) ){
		work_set_target_ratio(&miner_task->work, hash32);
		miner_task->work.data[NONCE_OFT32]=hitnonce;
       		applog(LOG_BLUE, "chip %d: bingo nonce!", id);
		return ID_NONCE_BINGO;
	}

       	if(hash32[7] > ptarget[7]) {
       		applog(LOG_ERR, "chip %d: hash error!", id);
		return ID_NONCE_HASH_ERR;
       	}
EXIT:	
	return ID_NONCE_REJECT;
}


