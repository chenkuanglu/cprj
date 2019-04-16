// upstream thread

#include "upstream.h"

void* upstream_thread(void *arg)
{
    (void)arg;
    chip_pack_t pack_buf;
    uint8_t *pack_ptr = (uint8_t *)&pack_buf;
    char ereason[128];
    
    size_t buf_size = sizeof(msg_header_t) + sizeof(chip_msg_t);
    uint8_t msg_buf[buf_size*2];
    
    msg_header_t *msg_header = (msg_header_t *)msg_buf;
    chip_msg_t *chip_msg = (chip_msg_t *)(msg_buf + sizeof(msg_header_t));
    
    pthread_setcancelstate(PTHREAD_CANCEL_DEFERRED, NULL);
    while (!IS_MINER_READY()) usleep(100*1000);

    for (;;) {
        // wait/read serial data 
        int rcv = 0, rcv_sum = 0;
        while (1) {
            pthread_testcancel();
			if ((rcv = ser_recieve(server_cb.fd_serial, &pack_ptr[rcv_sum], 1)) <= 0) {
                continue;
            }
            rcv_sum += rcv;
            if (rcv_sum >= 6) 
                break;
		}
        logi("UP FRM: %03d,0x%02x,0x%08x", pack_buf.id, pack_buf.reg, pack_buf.data);
        
        // chip id check 
        if ( (server_cb.opt_core_num > 0) && (pack_buf.id > server_cb.opt_core_num) ) {
            loge("error, invalid chip id %d", pack_buf.id);
            continue;
        }
        
        msg_header->sid_snd = BASE_SID + server_id;
        msg_header->algo_id = MSG_TYPE_UPSTREAM;
        msg_header->id  = 0;        // itself
        msg_header->cmd     = 0;                    // not use 
        gettimeofday(&(msg_header->time), NULL);
        msg_header->len     = sizeof(chip_msg_t);
        
        chip_msg->id = pack_buf.id;
        chip_msg->reg = pack_buf.reg;
        chip_msg->data = pack_buf.data;

        if (thrq_send(&server_cb.que_core, msg_buf, (sizeof(msg_header_t) + sizeof(chip_msg_t))) != 0) {
            sprintf(ereason, "thrq_send fail: upstream thread fail to send to 'que_core'");
            server_proper_exit(ereason);
            THREAD_RETIRE();
        }
    }
}

