//
#include "middles.h"
#include "fpga_commands.h"
#include <unistd.h>

#ifdef __cplusplus
extern "C"{
#endif

void init_middle_cb(middle_cb_t *mcb)
{
    for (int i = 0; i < REG_PERMUTATION_NUM; i++)
    {
        mcb->reg_permutation[i].id = 0;
        mcb->reg_permutation[i].reg = 0;
        mcb->reg_permutation[i].data = 0;
    }
    memset(&mcb->comb_stage_str[0], 0, COMB_STAGE_LEVEL_NUM + 1);

    for (int i = 0; i < 5; i++)
    {
        mcb->comb_stage_tbl[i] = 0;
    }

    mcb->clock_gating = 0;
    mcb->blake_conf = 0;
    mcb->hash_conf = ASIC_HASH_CONF_DEF;
    mcb->nonce_pos = ASIC_NONCE_POS_DEF;
    mcb->nonce_pos_ex = ASIC_NONCE_POS_EX_DEF;
    mcb->share_algo = 0;
    mcb->nonce_index = 0;
}

void set_reg_permutation(middle_cb_t *mcb, uint8_t id, uint32_t reg, uint32_t data)
{
    for (int i = 0; i < REG_PERMUTATION_NUM; i++)
    {
        if (mcb->reg_permutation[i].reg != 0)
        {
            if ( (mcb->reg_permutation[i].id == id) && (mcb->reg_permutation[i].reg == reg))
            {
                mcb->reg_permutation[i].data = data;
                break;
            }
        }
        else
        {
            mcb->reg_permutation[i].id = id;
            mcb->reg_permutation[i].reg = reg;
            mcb->reg_permutation[i].data = data;
            break;
        }
    }
}

// mid 1-8 , (0 all middle)
void set_clock_gating_middle(middle_cb_t *mcb, uint8_t chip_id, int mid, bool enable)
{
    if(mid >= 1 && mid <= 8)
    {
        if(enable)
        {
            switch(mid)
            {
                case 1:
                case 7:
                    if(enable)
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating | 0xFUL);  // [0] [1] [2] [3]
                    }
                    else
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating & (~0xFUL));  // [0] [1] [2] [3]
                    }
                    break;
                case 2:
                    if(enable)
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating | 0x11UL);  // [0] [4]
                    }
                    else
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating & (~0x11UL));  // [0] [4]
                    }
                    break;
                case 3:
                    if(enable)
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating | 0x20UL);  // [5]
                    }
                    else
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating & (~0x20UL));  // [5]
                    }
                    break;
                case 4:
                    if(enable)
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating | 0x40UL);  // [6]
                    }
                    else
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating & (~0x40UL));  // [6]
                    }
                    break;
                case 5:
                    if(enable)
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating | 0x80UL);  // [7]
                    }
                    else
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating & (~0x80UL));  // [7]
                    }
                    break;
                case 6:
                case 8:
                    if(enable)
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating | 0x100UL);  // [8]
                    }
                    else
                    {
                        set_clock_gating(mcb, chip_id, mcb->clock_gating & (~0x100UL));  // [8]
                    }
                    break;
            }
        }
    }
    else if(mid == 0)
    {
        if(enable)
        {
            set_clock_gating(mcb, chip_id, mcb->clock_gating | 0xffffffff );  
        }
        else
        {
            set_clock_gating(mcb, chip_id, mcb->clock_gating & 0);  
        }
    }
}

//
// 1=enable, 0=disable, default=0x1ff
//
// lbry myr xdag:       [0]=1
// lbry skein xdag:     [1]=1
// skein:               [2]=1
// lbry:                [3]=1
// myr-grl groestl:     [4]=1
// keccak:              [5]=1
// blake2s:             [6]=1
// lyra2rev2:           [7]=1
// comb:                [8]=1
void set_clock_gating(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 1;
    
    mcb->clock_gating = val;
    set_reg_permutation(mcb, asic_id, ASIC_REG_CLK_GATING, val);
}

//
// [0]: 1=lbry, 0=skein
// [1]: 1=myr-groestl, 0=groestl
// [2]: sha256 module shared, 1=lbry/skein, 0=myr/groestl
// [3]: 1=xdag, 0=[0]、[2] avail
void set_share_algo(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 1;
    
    mcb->share_algo = val;
    set_reg_permutation(mcb, asic_id, ASIC_REG_SHARE_ALGO, val);
}

void set_blake_conf(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
#ifndef ASIC_CR300
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 4;
#else
    uint8_t asic_id = chip_id;
#endif
    
    mcb->blake_conf = val;
    set_reg_permutation(mcb, asic_id, ASIC_REG_BLAKE_CONF, val);
}

// cr300 only
void set_multi_base(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
    uint8_t asic_id = chip_id;
    mcb->multi_base = val;
    set_reg_permutation(mcb, asic_id, 0x26, val);
}

void set_hash_conf(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
#ifndef ASIC_CR300
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 1;
#else
    uint8_t asic_id = chip_id;
#endif

    mcb->hash_conf = val;
    set_reg_permutation(mcb, asic_id, ASIC_REG_HASH_CONF, val);
}

// mid 1 2 3 7 8
void set_nonce_pos(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 1;
    
    mcb->nonce_pos = val;
    set_reg_permutation(mcb, asic_id, ASIC_REG_NONCE_POS, val);
}

// mid 4 5
void set_nonce_pos_ex(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 1;
    
    mcb->nonce_pos_ex = val;
    set_reg_permutation(mcb, asic_id, ASIC_REG_NONCE_POS_EX, val);
}

// mid 7 config
void set_mid7_config(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 1;
    
    mcb->mid7_config = val;
    set_reg_permutation(mcb, asic_id, ASIC_REG_MID7_CONFIG, val);
}

//
// middle6/comb sub-algo clock, 1=enable, 0=disable
//
// [0]: algo_none
// [1]~[5]      sha512      gost512     blake512    groestl512  jh512
// [6]~[10]     keccak512   skein512n   bmw512      luffa512    cubehash512
// [11]~[15]    shavite512  simd512     echo512     fugue512    hamsi512
// [16]~[17]    shabal512   whirlpool512
void set_comb_subclk(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 6;
    
    mcb->comb_sub_clk = val;
    set_reg_permutation(mcb, asic_id, COMB_REG_SUB_CLOCK, val);
}

// [4:0] nonce position, 0~31 word
// 
void set_comb_algomsg(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 6;
    asic_id = 0;
    
    mcb->comb_algo_msg.reg_algo_msg = val;
    set_reg_permutation(mcb, asic_id, COMB_REG_ALGO_MSG, val);
}

void set_comb_algohash(middle_cb_t *mcb, uint8_t chip_id, uint32_t val)
{
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 6;
    asic_id = 0;
    
    mcb->comb_algo_hash.reg_algo_hash = val;
    set_reg_permutation(mcb, asic_id, COMB_REG_ALGO_HASH, val);
}

//
// init the stage first(set to 0)
// for (;;) {
//     set_stage_item()
// }
//
// 47bits x 64 = 3008bits = stage_tbl
// item=47bits:
// [46:41]=case3, [40:35]=case2, [34:29]=case1, [28:23]=case0
// [22:14]=shift_R, [13:5]=shift_L, [4:0]=algo
//
// level = 0~63, algo=1~19 (0 is stop)
void set_stage_item(middle_cb_t *mcb, uint8_t chip_id, uint32_t level,
	uint32_t statge_algo,
	uint32_t shift_L, uint32_t shift_R,
	uint64_t case0, uint64_t case1, uint64_t case2, uint64_t case3)
{
    stage_tbl_t *stage = &mcb->comb_stage_tbl;
	uint8_t *pos = (uint8_t *)&((*stage)[0]);
	int byte_ix = 0;
	int bit_ix = 0;
	uint64_t word;
    //uint64_t value = 0;

    (void)chip_id;
	//uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 6;

    /*
    value |= statge_algo;
    value |= shift_L << 5;
    value |= shift_R << 14;
    value |= shift_L << 23;
    value |= shift_L << 23;
    */

	// set statge_algo
	//byte_ix = ((level * 47) + 0) / 8;
	//bit_ix = ((level * 47) + 0) % 8;
	byte_ix = ((level * 55) + 0) / 8;
	bit_ix = ((level * 55) + 0) % 8;
	word = *((int *)(pos + byte_ix));
	word |= statge_algo << bit_ix;
	*((uint64_t *)(pos + byte_ix)) = word;

	// set shift_L
	//byte_ix = ((level * 47) + 5) / 8;
	//bit_ix = ((level * 47) + 5) % 8;
	byte_ix = ((level * 55) + 5) / 8;
	bit_ix = ((level * 55) + 5) % 8;
	word = *((int *)(pos + byte_ix));
	word |= shift_L << bit_ix;
	*((uint64_t *)(pos + byte_ix)) = word;

	// set shift_R
	//byte_ix = ((level * 47) + 14) / 8;
	//bit_ix = ((level * 47) + 14) % 8;
	byte_ix = ((level * 55) + 14) / 8;
	bit_ix = ((level * 55) + 14) % 8;
	word = *((int *)(pos + byte_ix));
	word |= shift_R << bit_ix;
	*((uint64_t *)(pos + byte_ix)) = word;

	// set case 0~3
	//byte_ix = ((level * 47) + 23) / 8;
	//bit_ix = ((level * 47) + 23) % 8;
	byte_ix = ((level * 55) + 23) / 8;
	bit_ix = ((level * 55) + 23) % 8;
	word = *((int *)(pos + byte_ix));
	//word |= ((case3 << 18) | (case2 << 12) | (case1 << 6) | case0) << bit_ix;
	word |= ((case3 << 24) | (case2 << 16) | (case1 << 8) | case0) << bit_ix;
	*((uint64_t *)(pos + byte_ix)) = word;

    //int byte_start = ((level * 47) + 0) / 8;
    int byte_start = ((level * 55) + 0) / 8;
    int reg_ix;
    for (int i = 0; i < 3; i++)
    {
        reg_ix = (byte_start + i*4) / 4;
        //set_reg_permutation(mcb, asic_id, COMB_REG_STAGE_BASE + reg_ix , mcb->comb_stage_tbl[reg_ix]);
        //set_reg_permutation(mcb, asic_id, reg_ix + 0x41, mcb->comb_stage_tbl[reg_ix]);
        set_reg_permutation(mcb, 0, reg_ix + 0x41, mcb->comb_stage_tbl[reg_ix]);
    }
}

// 32 level, 5-bits every level (5x32 = 160 bits = 5 word)
// select an algorithm(0~17) for level used
void gen_stage_bits(stage_tbl_t *stage, uint32_t level, uint32_t statge_algo)
{
    uint32_t word_index = (level-1) / 32;
    uint32_t bit_low_index = ((level-1) * 5) % 32;
    uint32_t bit_high_index = ((level-1) * 5 + (5-1)) % 32;

    statge_algo &= ~COMB_STAGE_ALGO_MASK;

    (*stage)[word_index] |= statge_algo << bit_low_index;
    if (bit_high_index < (5-1))     // occupy 2 word
    {
        (*stage)[word_index] |= statge_algo >> ( (5-1) - bit_high_index );
    }
}

// eg. nist5 = blake + groestl + jh + keccak + skein = "\x03\x04\x05\x06\x07"
void gen_stage_table(middle_cb_t *mcb, void* stage_str)
{
    int i = 0;
    char *str = stage_str;

    memset(&mcb->comb_stage_tbl[0], 0, 5*sizeof(int));
    while (str[i] != 0)
    {
        gen_stage_bits(&mcb->comb_stage_tbl, COMB_STAGE_LEVEL_BASE + i, str[i]);
        i++;
    }
    
    strcpy(&mcb->comb_stage_str[0], str);
}

void set_comb_stagetbl(middle_cb_t *mcb, uint8_t chip_id, void* stage_str)
{
    uint8_t asic_id = ((chip_id - 1) / ASIC_MIDDLE_NUM) * ASIC_MIDDLE_NUM + 6;
    
    gen_stage_table(mcb, stage_str);
    
    for (int i = 0; i < 5; i++)
    {
        set_reg_permutation(mcb, asic_id, COMB_REG_STAGE_BASE + i, mcb->comb_stage_tbl[i]);
    }
}

void send_reg_permutation(int fd, middle_cb_t *mcb)
{
    int buf[109+1+440];
    int count = 0;
    int id;

    for (int i = 0; i < REG_PERMUTATION_NUM; i++)
    {
        if ( (mcb->reg_permutation[i].reg >= COMB_REG_STAGE_BASE) && (mcb->reg_permutation[i].reg <= COMB_REG_STAGE_END) ) {
            mcb->reg_permutation[i].reg = COMB_REG_STAGE_BASE;
        }

        if ( mcb->reg_permutation[i].reg != 0)
        {
            if (mcb->reg_permutation[i].reg == 0x41) {
                count++;
                buf[count] = mcb->reg_permutation[i].data;
                id = mcb->reg_permutation[i].id;
            } else {
                ASIC_WRITE_REG(fd, mcb->reg_permutation[i].id, mcb->reg_permutation[i].reg, mcb->reg_permutation[i].data);
                //usleep(50*1000);
            }
        }
        else
        {
            break;
        }
    }

    if (count) {
        printf("count = %d\n",count);
        buf[0] = count * 4;
        fpga_write_reg_buf(fd, id, 0x41, (uint8_t *)buf, (count+1)*4);
    }

    memset(mcb->reg_permutation, 0, sizeof(mcb->reg_permutation));
}


#ifdef __cplusplus
}
#endif

