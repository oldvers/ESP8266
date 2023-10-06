#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"

#include "types.h"
#include "led_task.h"
#include "led_strip.h"

//-------------------------------------------------------------------------------------------------

static QueueHandle_t gLedQueue   = {0};
static uint8_t       gCommand    = LED_CMD_EMPTY;
static uint32_t      gMax        = 0;
static uint32_t      gCounter    = 0;
static uint8_t       gLeds[16*3] = {0};

//-------------------------------------------------------------------------------------------------

static void led_Config(void)
{
    static uint8_t  col  = 1;
    static uint16_t led  = 0;
    static uint8_t  x[3] = {255, 0, 0};
    static uint16_t loop = 0;

    LED_Strip_SetColor(led, 0, 0, 0);
    led++;
    if (0 == (led %= (sizeof(gLeds) / 3)))
    {
        x[col++] = 0;
        col %= 3;
        x[col] = 255;
    }
    led %= (sizeof(gLeds) / 3);
    LED_Strip_SetColor(led, x[0], x[1], x[2]);
    LED_Strip_Update();

    loop++;
}

//-------------------------------------------------------------------------------------------------

static void led_Color(led_message_t * p_msg)
{
    uint16_t led  = 0;

    for (led = 0; led < (sizeof(gLeds) / 3); led++)
    {
        LED_Strip_SetColor(led, p_msg->red, p_msg->green, p_msg->blue);
    }
    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_ProcessMsg(led_message_t * p_msg)
{
    gCommand = p_msg->command;
    switch (gCommand)
    {
        case LED_CMD_CONFIG:
            gMax     = 3;
            gCounter = gMax;
            led_Config();
            break;
        case LED_CMD_COLOR:
            led_Color(p_msg);
            gCommand = LED_CMD_EMPTY;
            break;
        default:
            gCommand = LED_CMD_EMPTY;
            gMax     = 0;
            gCounter = 0;
            break;
    }
}

//-------------------------------------------------------------------------------------------------

static void led_Process(void)
{
    if (LED_CMD_EMPTY == gCommand) return;

    gCounter--;
    if (0 == gCounter)
    {
        switch (gCommand)
        {
            case LED_CMD_CONFIG:
                led_Config();
                break;
            case LED_CMD_COLOR:
                break;
            default:
                gCommand = LED_CMD_EMPTY;
                gMax     = 0;
                gCounter = 0;
                break;
        }
        gCounter = gMax;
    }
}

//-------------------------------------------------------------------------------------------------

static void led_Task(void * pvParameters)
{
    BaseType_t    status = pdFAIL;
    led_message_t msg    = {0};

    printf("LED Task started...\n");
    LED_Strip_Init(gLeds, sizeof(gLeds));

    while (FW_TRUE)
    {
        status = xQueueReceive(gLedQueue, (void *)&msg, 10 / portTICK_RATE_MS);

        if (pdTRUE == status)
        {
            led_ProcessMsg(&msg);
        }
        else
        {
            led_Process();
        }
    }
}

//-------------------------------------------------------------------------------------------------

void LED_Task_Init(void)
{
    gLedQueue = xQueueCreate(20, sizeof(led_message_t));

    (void)xTaskCreate(led_Task, "LED_Task", 1024, NULL, 10, NULL);
}

//-------------------------------------------------------------------------------------------------

void LED_Task_SendMsg(led_message_t * p_msg)
{
    (void)xQueueSendToBack(gLedQueue, (void *)p_msg, (TickType_t)0);
}

//-------------------------------------------------------------------------------------------------
