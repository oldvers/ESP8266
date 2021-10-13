#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/uart.h"

#include "east_uart.h"
#include "east_packet.h"
#include "block_queue.h"

#define EAST_MAX_DATA_LENGTH   (200)
#define EAST_HEADER_LENGTH     (6)
#define EAST_MAX_PACKET_LENGTH (EAST_HEADER_LENGTH + EAST_MAX_DATA_LENGTH)

#define EVT_EAST_TX_COMPLETE   (1 << 0)

EventGroupHandle_t eastEvents;
static BlockQueue_p iQueue;
static EAST_p iEast = NULL;
static EAST_p oEast = NULL;
static U8 iEastCtnr[16] = {0};
static U8 oEastCtnr[16] = {0};
static U8 iBuffer[EAST_MAX_DATA_LENGTH * 8] = {0};
static U8 oBuffer[EAST_MAX_PACKET_LENGTH] = {0};

static FW_BOOLEAN oEAST_GetByte(U8 * pValue)
{
    FW_BOOLEAN result = FW_FALSE;

    /* If there are some data in EAST packet */
    if (0 < EAST_GetPacketSize(oEast))
    {
        (void)EAST_GetByte(oEast, pValue);
        result = FW_TRUE;
    }

    return result;
//    FW_RESULT r = FW_ERROR;

    /* Empty the EAST block */
//    r = EAST_GetByte(oEast, pValue);
//    if (FW_COMPLETE == r)
//    {
//        if (FW_TRUE == IRQ_IsInExceptionMode())
//        {
//            (void)xEventGroupSetBits(eastEvents, EVT_EAST_TX_COMPLETE);
//        }
//        else
//        {
//            /* We have not woken a task at the start of the ISR */
//            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

//            (void)xEventGroupSetBitsFromISR
//            (
//                eastEvents,
//                EVT_EAST_TX_COMPLETE,
//                &xHigherPriorityTaskWoken
//            );

//            /* Now we can request to switch context if necessary */
//            if (xHigherPriorityTaskWoken)
//            {
//                taskYIELD();
//            }
//        }
//    }
//    return FW_FALSE;
}

static FW_BOOLEAN iEAST_PutByte(U8 * pValue)
{
    FW_RESULT r = FW_ERROR;
    U8 * buffer = NULL;
    U32 size = 0;

    /* Fill the EAST block */
    r = EAST_PutByte(iEast, *pValue);
    if (FW_COMPLETE == r)
    {
        /* If the block queue is full - ignore */
        if (0 == BlockQueue_GetCountOfFree(iQueue))
        {
            return FW_FALSE;
        }

        /* Put the block into the queue */
        r = BlockQueue_Enqueue(iQueue, EAST_GetDataSize(iEast));
        if (FW_SUCCESS == r)
        {
            /* Allocate the memory for the next block */
            r = BlockQueue_Allocate(iQueue, &buffer, &size);
            if (FW_SUCCESS == r)
            {
                (void)EAST_SetBuffer(iEast, buffer, size);
            }
        }
    }

    return FW_TRUE;
}

static FW_BOOLEAN iEAST_Complete(U8 * pValue)
{
    return FW_TRUE;
}

static FW_BOOLEAN oEAST_Complete(U8 * pValue)
{
    /* We have not woken a task at the start of the ISR */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    (void)xEventGroupSetBitsFromISR
    (
        eastEvents,
        EVT_EAST_TX_COMPLETE,
        &xHigherPriorityTaskWoken
    );

    /* Now we can request to switch context if necessary */
    if (xHigherPriorityTaskWoken)
    {
        taskYIELD();
    }

    return FW_TRUE;
}

void vEAST_SendResponse(U8 * pRsp, U16 size)
{
    EventBits_t events = 0;

    /* Reset the EAST */
    EAST_SetBuffer(oEast, pRsp, size);

    /* Send the response */
    EAST_UART_TxStart();

    /* Wait for transmitting complete */
    events = xEventGroupWaitBits
             (
                 eastEvents,
                 EVT_EAST_TX_COMPLETE,
                 pdTRUE,
                 pdFALSE,
                 portMAX_DELAY
             );
    if (EVT_EAST_TX_COMPLETE == (events & EVT_EAST_TX_COMPLETE))
    {
        /* Check for errors */
    }
}

static void vEAST_Task(void * pvParameters)
{
    U8 * req = NULL, * rsp = oBuffer;
    U32 size = 0;

    //U8 * rxData = (U8 *) malloc(BUF_SIZE);
    //U8 * txData = (U8 *) malloc(BUF_SIZE);
    //U32 r = 0, t = 0;

    printf("EAST Task started...\n");

    // Init the EAST packets


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
    printf("EAST Task init the UART...\n");
    EAST_UART_Init(921600, iEAST_PutByte, iEAST_Complete, oEAST_GetByte, oEAST_Complete);
    EAST_UART_RxStart();

    printf("EAST Task enter iteration...\n");
    while (FW_TRUE)
    {
        /* Dequeue the request */
        (void)BlockQueue_Dequeue(iQueue, (U8 **)&req, &size);

        printf("EAST Task received the packet. Len = %d\n", size);

        /* Prepare the response */
        memcpy(rsp, req, size);
        /* Process the request */
        /* Send the response */
        vEAST_SendResponse(rsp, size);
        printf("EAST Task the packet sent. Len = %d\n", size);

        /* Release the block */
        (void)BlockQueue_Release(iQueue);

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
//        vTaskDelay(500);
//        int x = EAST_UART_GetRx();
//        if (0 < x)
//        {
//            printf("EAST Task Rx Len = %d\n", x);
//        }
    }
    //vTaskDelete(NULL);
}

void EAST_Task_Init()
{
    U8 * buffer = NULL;
    U32 size = 0;

    /* Initialize EAST packet containers */
    iEast = EAST_Init(iEastCtnr, sizeof(iEastCtnr), NULL, 0);
    oEast = EAST_Init(oEastCtnr, sizeof(oEastCtnr), oBuffer, sizeof(oBuffer));
    /* Initialize the EAST packet queue */
    iQueue = BlockQueue_Init(iBuffer, sizeof(iBuffer), EAST_MAX_DATA_LENGTH);
    /* Allocate the memory for the first input EAST packet */
    (void)BlockQueue_Allocate(iQueue, &buffer, &size);
    /* Setup/Reset the input EAST packet */
    (void)EAST_SetBuffer(iEast, buffer, size);

    /* Create the event group for synchronization */
	eastEvents = xEventGroupCreate();
    (void)xEventGroupClearBits(eastEvents, EVT_EAST_TX_COMPLETE);

    xTaskCreate(vEAST_Task, "EAST_Task", 1024, NULL, 10, NULL);
}
