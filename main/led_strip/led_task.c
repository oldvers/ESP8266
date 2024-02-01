#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"

#include "types.h"
#include "led_task.h"
#include "led_strip.h"

//-------------------------------------------------------------------------------------------------

enum
{
    RED = 0,
    GREEN,
    BLUE,
    PIXEL_LED_COUNT
};

typedef void (* iterate_fp_t)(void);

typedef struct
{
    uint8_t       command;
    uint8_t       pixel[PIXEL_LED_COUNT];
    uint8_t       maxTimeCount;
    uint8_t       timeCounter;
    uint8_t       index;
    uint8_t       padding;
    iterate_fp_t  fpIterate;
    uint8_t       buffer[16*3];
} leds_t;

//-------------------------------------------------------------------------------------------------

static QueueHandle_t gLedQueue = {0};
static leds_t        gLeds     = {0};

//-------------------------------------------------------------------------------------------------
//--- Simple Color Indication ---------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_Color(void)
{
    uint16_t led  = 0;

    for (led = 0; led < (sizeof(gLeds.buffer) / 3); led++)
    {
        LED_Strip_SetColor(led, gLeds.pixel[RED], gLeds.pixel[GREEN], gLeds.pixel[BLUE]);
    }
    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_Color(void)
{
    gLeds.command   = LED_CMD_EMPTY;
    gLeds.fpIterate = led_IterateIndication_Color;
    gLeds.fpIterate();
}

//-------------------------------------------------------------------------------------------------
//--- Running LED Indication ----------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_Run(void)
{
    static uint8_t  index                  = 1;
    static uint16_t led                    = 0;
    static uint8_t  pixel[PIXEL_LED_COUNT] = {255, 0, 0};

    LED_Strip_SetColor(led, 0, 0, 0);
    led = ((led + 1) % (sizeof(gLeds.buffer) / 3)); 
    /* Switch the color R -> G -> B */
    if (0 == led)
    {
        pixel[index++] = 0;
        index %= 3;
        pixel[index] = 255;
    }
    /* Set the color depending on color settings */
    if (0 == (gLeds.pixel[RED] || gLeds.pixel[GREEN] || gLeds.pixel[BLUE]))
    {
        LED_Strip_SetColor(led, pixel[RED], pixel[GREEN], pixel[BLUE]);
    }
    else
    {
        LED_Strip_SetColor(led, gLeds.pixel[RED], gLeds.pixel[GREEN], gLeds.pixel[BLUE]);
    }

    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_Run(void)
{
    gLeds.maxTimeCount = 3;
    gLeds.timeCounter  = gLeds.maxTimeCount;
    gLeds.fpIterate    = led_IterateIndication_Run;
    gLeds.fpIterate();
}

//-------------------------------------------------------------------------------------------------
//--- Fade LED Indication -------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_Fade(void)
{
//    static uint8_t  * pFadeColor    = NULL;
//    static uint16_t * pMaxFadeValue = NULL;
//    static uint8_t  pixel[COUNT] = {255, 0, 0};
//
//    if (NULL == pFadeColor)
//    {
//        if (0 < gLeds.red)
//        {
//            maxFade = gLeds.red;
//        }
//    }
//
//
//
//    LED_Strip_SetColor(led, 0, 0, 0);
//    led = ((led + 1) % (sizeof(gLeds.buffer) / 3)); 
//    /* Switch the color R -> G -> B */
//    if (0 == led)
//    {
//        pixel[index++] = 0;
//        index %= 3;
//        pixel[index] = 255;
//    }
//    /* Set the color depending on color settings */
//    if (0 == (gLeds.red || gLeds.green || gLeds.blue))
//    {
//        LED_Strip_SetColor(led, pixel[RED], pixel[GREEN], pixel[BLUE]);
//    }
//    else
//    {
//        LED_Strip_SetColor(led, gLeds.red, gLeds.green, gLeds.blue);
//    }
//
//    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_Fade(void)
{
    if (0 < gLeds.pixel[RED])
    {
        gLeds.index = RED;
    }
    else if (0 < gLeds.pixel[GREEN])
    {
        gLeds.index = GREEN;
    }
    else if (0 < gLeds.pixel[BLUE])
    {
        gLeds.index = BLUE;
    }
    else
    {
        gLeds.index = PIXEL_LED_COUNT;
    }

    if (PIXEL_LED_COUNT != gLeds.index)
    {
        gLeds.maxTimeCount = 2;
        gLeds.timeCounter  = gLeds.maxTimeCount;
        memset(gLeds.buffer, 0, sizeof(gLeds.buffer));
        gLeds.fpIterate    = led_IterateIndication_Fade;
        gLeds.fpIterate();
    }
    else
    {
        gLeds.command      = LED_CMD_EMPTY;
        gLeds.fpIterate    = NULL;
        gLeds.maxTimeCount = 0;
        gLeds.timeCounter  = 0;
    }
}

//-------------------------------------------------------------------------------------------------

static void led_ProcessMsg(led_message_t * p_msg)
{
    gLeds.command      = p_msg->command;
    gLeds.pixel[RED]   = p_msg->red;
    gLeds.pixel[GREEN] = p_msg->green;
    gLeds.pixel[BLUE]  = p_msg->blue;
    switch (gLeds.command)
    {
        case LED_CMD_INDICATE_COLOR:
            led_SetIndication_Color();
            break;
        case LED_CMD_INDICATE_RUN:
            led_SetIndication_Run();
            break;
        case LED_CMD_INDICATE_FADE:
            led_SetIndication_Fade();
            break;
        default:
            gLeds.command      = LED_CMD_EMPTY;
            gLeds.fpIterate    = NULL;
            gLeds.maxTimeCount = 0;
            gLeds.timeCounter  = 0;
            break;
    }
}

//-------------------------------------------------------------------------------------------------

static void led_Process(void)
{
    if (LED_CMD_EMPTY == gLeds.command) return;

    gLeds.timeCounter--;
    if (0 == gLeds.timeCounter)
    {
        if (NULL != gLeds.fpIterate)
        {
            gLeds.fpIterate();
        }
        gLeds.timeCounter = gLeds.maxTimeCount;
    }
}

//-------------------------------------------------------------------------------------------------

static void led_Task(void * pvParameters)
{
    BaseType_t    status = pdFAIL;
    led_message_t msg    = {0};

    printf("LED Task started...\n");
    LED_Strip_Init(gLeds.buffer, sizeof(gLeds.buffer));
    vTaskDelay(30 / portTICK_RATE_MS);
    led_SetIndication_Color();
    vTaskDelay(30 / portTICK_RATE_MS);
    led_SetIndication_Color();

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
