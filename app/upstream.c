// upstream thread

#include "upstream.h"
#include "trigg_miner.h"

void* thread_upstream(void *arg)
{
    up_frame_t pack_buf;
    uint8_t *pack_ptr = (uint8_t *)&pack_buf;
    
    const int buf_size = sizeof(core_msg_t) + sizeof(upstream_t);
    char msg_buf[buf_size];
    
    core_msg_t *msg_header = (core_msg_t *)msg_buf;
    upstream_t *upstream = (upstream_t *)(msg_header->data);

    if (triggm.fd_dev <= 0) {
        sloge(CLOG, "Invalid serial fd %d\n", triggm.fd_dev);
        CORE_THR_RETIRE();
    }
    
    for (;;) {
        int rcv = 0, rcv_sum = 0;
        while (1) {
            pthread_testcancel();
			if ((rcv = ser_receive(triggm.fd_dev, &pack_ptr[rcv_sum], 1)) <= 0) {
                nsleep(0.01);
                continue;
            }
            rcv_sum += rcv;
            if (rcv_sum >= 6) 
                break;
		}
        slogi(CLOG, "UP FRM: %03d,0x%02x,0x%08x\n", pack_buf.id, pack_buf.addr, pack_buf.data);
        
        // chip id check 
        if ( (triggm.chip_num > 0) && (pack_buf.id <= 0 || pack_buf.id > triggm.chip_num) ) {
            sloge(CLOG, "Invalid chip id %03d\n", pack_buf.id);
            continue;
        }
        
        msg_header->type = 0;
        msg_header->cmd = TRIGG_CMD_UP_FRM;
        msg_header->tm  = monotime();
        msg_header->len = sizeof(upstream_t);
        
        upstream->id = pack_buf.id;
        upstream->addr = pack_buf.addr;
        upstream->data = pack_buf.data;

        if (thrq_send(&triggm.thrq_miner, msg_buf, buf_size) != 0) {
            sloge(CLOG, "upstream send msg fail\n");
            nsleep(0.1);
        }
    }
}

