idf_component_register(SRCS ./main.c east/east_packet.c east/east_task.c east/east_uart.c block_queue/block_queue.c wifi/wifi_task.c
                       INCLUDE_DIRS . east/include block_queue/include wifi/include                       # Edit following two lines to set component requirements (see docs)
                       REQUIRES 
                       PRIV_REQUIRES )

