#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"

#include "east_uart.h"
#include "east_packet.h"

#define BUF_SIZE (256)

static EAST_p gRxEast = NULL;
static U8 gRxEastContainer[16] = {0};
static U8 gRxBuffer[BUF_SIZE] = {0};
static EAST_p gTxEast = NULL;
static U8 gTxEastContainer[16] = {0};
static U8 gTxBuffer[BUF_SIZE] = {0};
static U32 gCount = 0;

static FW_BOOLEAN vEAST_TxGetByte(U8 * pValue)
{
    return FW_FALSE;
}

static FW_BOOLEAN vEAST_RxPutByte(U8 * pValue)
{
    gCount++;
    return FW_TRUE;
}

static FW_BOOLEAN vEAST_RxComplete(U8 * pValue)
{
    return FW_TRUE;
}

static FW_BOOLEAN vEAST_TxComplete(U8 * pValue)
{
    return FW_TRUE;
}

static void vEAST_Task(void * pvParameters)
{
    U8 * rxData = (U8 *) malloc(BUF_SIZE);
    U8 * txData = (U8 *) malloc(BUF_SIZE);
    U32 r = 0, t = 0;

    printf("EAST Task started...\n");

    // Init the EAST packets
    gRxEast = EAST_Init(gRxEastContainer, sizeof(gRxEastContainer), gRxBuffer, sizeof(gRxBuffer));
    gTxEast = EAST_Init(gTxEastContainer, sizeof(gTxEastContainer), gTxBuffer, sizeof(gTxBuffer));

    // Configure parameters of an UART driver,
    // communication pins and install the driver
 //   uart_config_t uart_config =
 //   {
 //       .baud_rate = 921600,
 //       .data_bits = UART_DATA_8_BITS,
 //       .parity    = UART_PARITY_DISABLE,
 //       .stop_bits = UART_STOP_BITS_1,
 //       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
 //   };
 //   uart_param_config(UART_NUM_0, &uart_config);
 //   uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
    EAST_UART_Init(921600, vEAST_RxPutByte, vEAST_RxComplete, vEAST_TxGetByte, vEAST_TxComplete);

    printf("EAST Task enter iteration...\n");
    while (FW_TRUE)
    {
        // Read data from the UART
//        int len = uart_read_bytes(UART_NUM_0, rxData, BUF_SIZE, 20 / portTICK_RATE_MS);
//        if (0 < len)
//        {
//            for (r = 0; r < len; r++)
//            {
//                if (FW_COMPLETE == EAST_PutByte(gRxEast, rxData[r]))
//                {
//                    printf("EAST packet received. Len = %d\n", EAST_GetDataSize(gRxEast));
//                    gTxBuffer[0] = gRxBuffer[0];
//                    gTxBuffer[1] = 0;
//                    gTxBuffer[2] = gRxBuffer[2];
//                    gTxBuffer[3] = gRxBuffer[3];
//                    EAST_SetBuffer(gTxEast, gTxBuffer, 4);
//                    for (t = 0; t < BUF_SIZE; t++)
//                    {
//                        printf("Byte = %02X\n", txData[t]);
//                        if (FW_COMPLETE == EAST_GetByte(gTxEast, &txData[t]))
//                        {
//                            uart_write_bytes(UART_NUM_0, (const char *)txData, (t + 1));
//                            printf("EAST packet sent. Len = %d\n", EAST_GetDataSize(gTxEast));
//                            break;
//                        }
//                    }
//
//                }
//            }
//        }
        vTaskDelay(500);
//        int x = EAST_UART_GetRx();
//        if (0 < x)
//        {
//            printf("EAST Task Rx Len = %d\n", x);
//        }
        printf("EAST Task iterate. Count = %d\n", gCount);
    }
}

void EAST_Task_Init()
{
    xTaskCreate(vEAST_Task, "EAST_Task", 1024, NULL, 10, NULL);
}
