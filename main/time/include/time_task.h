#ifndef __TIME_TASK_H__
#define __TIME_TASK_H__

typedef enum
{
    TIME_CMD_EMPTY,
    TIME_CMD_SUN_ENABLE,
    TIME_CMD_SUN_DISABLE,
} time_command_t;

typedef struct
{
    time_command_t command;
} time_message_t;

void Time_Task_Init(void);
void Time_Task_SendMsg(time_message_t * p_msg);
void Time_Task_Test(void);

#endif /* __TIME_TASK_H__ */
