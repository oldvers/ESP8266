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

typedef void (* iterate_fp_t)(void);

typedef struct
{
    double h;
    double s;
    double v;
} hsv_t;

typedef struct
{
    led_command_t command;
    led_color_t   dst_color;
    led_color_t   src_color;
    uint32_t      time_interval;
    uint32_t      time_counter;
    uint16_t      offset;
    uint16_t      led;
    hsv_t         hsv;
    iterate_fp_t  fp_iterate;
    uint8_t       buffer[LED_TASK_PIXELS_COUNT * 3];
} leds_t;

//-------------------------------------------------------------------------------------------------

static QueueHandle_t gLedQueue = {0};
static leds_t        gLeds     = {0};

//-------------------------------------------------------------------------------------------------

static void led_RGBtoHSV(led_color_t * p_color, hsv_t * p_hsv)
{
    double min, max, delta;

    min = (p_color->r < p_color->g) ? p_color->r : p_color->g;
    min = (min < p_color->b) ? min : p_color->b;

    max = (p_color->r > p_color->g) ? p_color->r : p_color->g;
    max = (max > p_color->b) ? max : p_color->b;

    /* Value */
    p_hsv->v = max / 255.0;

    delta = max - min;

    if (max != 0)
    {
        /* Saturation */
        p_hsv->s = (1.0 * delta / max); 
    }
    else
    {
        /* r = g = b = 0 */
        /* s = 0, v is undefined */
        p_hsv->s = 0;
        p_hsv->h = 0;
        return;
    }

    /* Hue */
    if (p_color->r == max)
    {
        p_hsv->h = (p_color->g - p_color->b) / delta;
    }
    else if (p_color->g == max)
    {
        p_hsv->h = 2 + (p_color->b - p_color->r) / delta;
    }
    else
    {
        p_hsv->h = 4 + (p_color->r - p_color->g) / delta;
    }

    /* Convert hue to degrees and back */
    p_hsv->h *= 60; 
    if (p_hsv->h < 0)
    {
        p_hsv->h += 360;
    }
    p_hsv->h /= 360;
}

//-------------------------------------------------------------------------------------------------

static void led_HSVtoRGB(hsv_t * p_hsv, led_color_t * p_color)
{
    double r = 0, g = 0, b = 0;

    int i = (int)(p_hsv->h * 6);
    double f = p_hsv->h * 6 - i;
    double p = p_hsv->v * (1 - p_hsv->s);
    double q = p_hsv->v * (1 - f * p_hsv->s);
    double t = p_hsv->v * (1 - (1 - f) * p_hsv->s);

    switch(i)
    {
        case 0: r = p_hsv->v, g = t, b = p; break;
        case 1: r = q, g = p_hsv->v, b = p; break;
        case 2: r = p, g = p_hsv->v, b = t; break;
        case 3: r = p, g = q, b = p_hsv->v; break;
        case 4: r = t, g = p, b = p_hsv->v; break;
        case 5: r = p_hsv->v, g = p, b = q; break;
    }

    p_color->r = (uint8_t)(r * 255);
    p_color->g = (uint8_t)(g * 255);
    p_color->b = (uint8_t)(b * 255);
}

//-------------------------------------------------------------------------------------------------

// Performs linear interpolation between two values
static double led_LinearInterpolation(double a, double b, double t)
{
    return a + (b - a) * t;
}

//-------------------------------------------------------------------------------------------------

// Performs smooth color transition between two RGB colors
static void led_SmoothColorTransition
(
    led_color_t * p_a,
    led_color_t * p_b,
    double prgs,
    led_color_t * p_r
)
{
    // Clamp progress value between 0 and 1
    if (prgs < 0) prgs = 0;
    if (prgs > 1) prgs = 1;

    // Interpolate each RGB component separately
    p_r->r = (uint8_t)led_LinearInterpolation(p_a->r, p_b->r, prgs);
    p_r->g = (uint8_t)led_LinearInterpolation(p_a->g, p_b->g, prgs);
    p_r->b = (uint8_t)led_LinearInterpolation(p_a->b, p_b->b, prgs);
}

//-------------------------------------------------------------------------------------------------
//--- Simple Color Indication ---------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_Color(void)
{
    led_color_t result  = {0};
    double      percent = gLeds.offset * 0.01;

    if ((gLeds.dst_color.dword != gLeds.src_color.dword) && (100 > gLeds.offset))
    {
        led_SmoothColorTransition(&gLeds.src_color, &gLeds.dst_color, percent, &result);
        LED_Strip_SetColor(&result);
        gLeds.offset += 3;
    }
    else
    {
        LED_Strip_SetColor(&gLeds.dst_color);
        gLeds.src_color.dword = gLeds.dst_color.dword;
        gLeds.command = LED_CMD_EMPTY;
    }
    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_Color(led_message_t * p_msg)
{
    gLeds.dst_color.dword = 0;
    gLeds.dst_color.r     = p_msg->dst_color.r;
    gLeds.dst_color.g     = p_msg->dst_color.g;
    gLeds.dst_color.b     = p_msg->dst_color.b;
    gLeds.src_color.dword = 0;
    gLeds.offset          = 0;
    gLeds.time_interval   = 3;
    gLeds.time_counter    = gLeds.time_interval;

    LED_Strip_GetAverageColor(&gLeds.src_color);

    gLeds.fp_iterate = led_IterateIndication_Color;
    gLeds.fp_iterate();
}

//-------------------------------------------------------------------------------------------------
//--- Running LED Indication ----------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_RgbCirculation(void)
{
    LED_Strip_Update();
    LED_Strip_Rotate(false);
    if (UINT16_MAX != gLeds.led)
    {
        gLeds.led = ((gLeds.led + 1) % (sizeof(gLeds.buffer) / 3)); 
        /* Switch the color R -> G -> B */
        if (0 == gLeds.led)
        {
            gLeds.dst_color.bytes[gLeds.offset++] = 0;
            gLeds.offset %= 3;
            gLeds.dst_color.bytes[gLeds.offset] = UINT8_MAX;
            LED_Strip_SetPixelColor(gLeds.led, &gLeds.dst_color);
        }
    }
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_RgbCirculation(led_message_t * p_msg)
{
    LED_Strip_Clear();

    gLeds.dst_color.dword = 0;
    gLeds.dst_color.r     = p_msg->dst_color.r;
    gLeds.dst_color.g     = p_msg->dst_color.g;
    gLeds.dst_color.b     = p_msg->dst_color.b;

    /* Set the color depending on color settings */
    if (0 == gLeds.dst_color.dword)
    {
        gLeds.offset      = 0;
        gLeds.led         = 0;
        gLeds.dst_color.r = UINT8_MAX;
        LED_Strip_SetPixelColor(gLeds.led, &gLeds.dst_color);
    }
    else
    {
        gLeds.offset  = UINT8_MAX;
        gLeds.led     = UINT16_MAX;
        LED_Strip_SetPixelColor(0, &gLeds.dst_color);
    }

    gLeds.time_interval = 3;
    gLeds.time_counter  = gLeds.time_interval;
    gLeds.fp_iterate    = led_IterateIndication_RgbCirculation;
    gLeds.fp_iterate();
}

//-------------------------------------------------------------------------------------------------
//--- Fade LED Indication -------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_Fade(void)
{
    enum
    {
        MAX_FADE_LEVEL = 30,
    };

    led_HSVtoRGB(&gLeds.hsv, &gLeds.dst_color);
    LED_Strip_SetColor(&gLeds.dst_color);
    LED_Strip_Update();

    gLeds.led++;
    if (MAX_FADE_LEVEL == gLeds.led)
    {
        gLeds.led    = 0;
        gLeds.offset = ((gLeds.offset + 1) % 2);
    }
    if (0 == gLeds.offset)
    {
        gLeds.hsv.v = (gLeds.led * 0.02);
    }
    else
    {
        gLeds.hsv.v = ((MAX_FADE_LEVEL - gLeds.led - 1) * 0.02);
    }
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_Fade(led_message_t * p_msg)
{
    LED_Strip_Clear();

    gLeds.dst_color.dword = 0;
    gLeds.dst_color.r     = p_msg->dst_color.r;
    gLeds.dst_color.g     = p_msg->dst_color.g;
    gLeds.dst_color.b     = p_msg->dst_color.b;

    led_RGBtoHSV(&gLeds.dst_color, &gLeds.hsv);
    gLeds.hsv.v         = 0.0;
    gLeds.offset        = 0;
    gLeds.led           = 0;
    gLeds.time_interval = 2;
    gLeds.time_counter  = gLeds.time_interval;
    gLeds.fp_iterate    = led_IterateIndication_Fade;
    gLeds.fp_iterate();
}

//-------------------------------------------------------------------------------------------------
//--- PingPong LED Indication ---------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_PingPong(void)
{
    LED_Strip_Update();
    LED_Strip_Rotate(0 == gLeds.offset);

    gLeds.led++;
    if ((sizeof(gLeds.buffer) / 3) == gLeds.led)
    {
        gLeds.led    = 0;
        gLeds.offset = ((gLeds.offset + 1) % 2);
    }
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_PingPong(led_message_t * p_msg)
{
    LED_Strip_Clear();

    gLeds.dst_color.dword = 0;
    gLeds.dst_color.r     = p_msg->dst_color.r;
    gLeds.dst_color.g     = p_msg->dst_color.g;
    gLeds.dst_color.b     = p_msg->dst_color.b;
    gLeds.offset          = 0;
    gLeds.led             = 0;
    LED_Strip_SetPixelColor(gLeds.led, &gLeds.dst_color);
    gLeds.time_interval = 3;
    gLeds.time_counter  = gLeds.time_interval;
    gLeds.fp_iterate    = led_IterateIndication_PingPong;
    gLeds.fp_iterate();
}

//-------------------------------------------------------------------------------------------------
//--- Rainbow LED Indication ---------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_RainbowCirculation(void)
{
    LED_Strip_Rotate(false);
    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_RainbowCirculation(led_message_t * p_msg)
{
    double max = 0.0;

    gLeds.dst_color.dword = 0;
    gLeds.dst_color.r     = p_msg->dst_color.r;
    gLeds.dst_color.g     = p_msg->dst_color.g;
    gLeds.dst_color.b     = p_msg->dst_color.b;

    if (0 == gLeds.dst_color.dword)
    {
        /* Running Rainbow */
        max = 0.222;
        /* Set iteration period and callback */
        gLeds.time_interval = 5;
        gLeds.time_counter  = gLeds.time_interval;
        gLeds.fp_iterate    = led_IterateIndication_RainbowCirculation;
    }
    else
    {
        /* Static Rainbow */
        max = (gLeds.dst_color.r > gLeds.dst_color.g) ? gLeds.dst_color.r : gLeds.dst_color.g;
        max = (max > gLeds.dst_color.b) ? max : gLeds.dst_color.b;
        max /= 255.0;
        /* Disable iteration */
        gLeds.command       = LED_CMD_EMPTY;
        gLeds.time_interval = 0;
        gLeds.time_counter  = 0;
        gLeds.fp_iterate    = NULL;
    }

    /* Draw the Rainbow */
    gLeds.hsv.s = 1.0;
    gLeds.hsv.v = max;
    for (gLeds.led = 0; gLeds.led < (sizeof(gLeds.buffer) / 3); gLeds.led++)
    {
        gLeds.hsv.h = ((gLeds.led + 0.5) * 1.0 / (sizeof(gLeds.buffer) / 3));
        led_HSVtoRGB(&gLeds.hsv, &gLeds.dst_color);
        LED_Strip_SetPixelColor(gLeds.led, &gLeds.dst_color);
    }
    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_ProcessMsg(led_message_t * p_msg)
{
    gLeds.command         = p_msg->command;
    switch (gLeds.command)
    {
        case LED_CMD_INDICATE_COLOR:
            led_SetIndication_Color(p_msg);
            break;
        case LED_CMD_INDICATE_RGB_CIRCULATION:
            led_SetIndication_RgbCirculation(p_msg);
            break;
        case LED_CMD_INDICATE_FADE:
            led_SetIndication_Fade(p_msg);
            break;
        case LED_CMD_INDICATE_PINGPONG:
            led_SetIndication_PingPong(p_msg);
            break;
        case LED_CMD_INDICATE_RAINBOW_CIRCULATION:
            led_SetIndication_RainbowCirculation(p_msg);
            break;
        default:
            gLeds.command       = LED_CMD_EMPTY;
            gLeds.fp_iterate    = NULL;
            gLeds.time_interval = 0;
            gLeds.time_counter  = 0;
            break;
    }
}

//-------------------------------------------------------------------------------------------------

static void led_Process(void)
{
    if (LED_CMD_EMPTY == gLeds.command) return;

    gLeds.time_counter--;
    if (0 == gLeds.time_counter)
    {
        if (NULL != gLeds.fp_iterate)
        {
            gLeds.fp_iterate();
        }
        gLeds.time_counter = gLeds.time_interval;
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
    LED_Strip_Clear();
    LED_Strip_Update();
    vTaskDelay(30 / portTICK_RATE_MS);
    LED_Strip_Clear();
    LED_Strip_Update();

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
