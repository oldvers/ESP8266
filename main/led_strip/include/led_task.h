#ifndef __LED_TASK_H__
#define __LED_TASK_H__

#include <stdint.h>

#define LED_CMD_EMPTY      (0xFF)
#define LED_CMD_CONFIG     (0x00)
#define LED_CMD_COLOR      (0x01)

typedef struct
{
    uint8_t command;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} led_message_t;

void LED_Task_Init(void);
void LED_Task_SendMsg(led_message_t * p_msg);

#endif /* __LED_TASK_H__ */