#ifndef __UDP_TASK_H__
#define __UDP_TASK_H__

void UDP_Task_Init(void);
void UDP_NotifyWiFiIsConnected(uint32_t ip);
void UDP_NotifyWiFiIsDisconnected(void);

#endif /* __UDP_TASK_H__ */
