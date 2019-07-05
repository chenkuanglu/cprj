/**
 * @file    user_app.c
 * @author  ln
 * @brief   app 模板工程: applications
 *
 * run: build/app/template/template --opt1 a1 --opt2 b2 c3
 */
 
#include "../../common/common.h"
#include "user_app.h"

#define EVENT_ID_PRINT      100

void event_print(void *arg)
{
    logi("app is running...\n");
}

int app_init(void)
{
    TMR_ADD(EVENT_ID_PRINT, TMR_EVENT_TYPE_PERIODIC, 1.0, event_print, NULL);
}
