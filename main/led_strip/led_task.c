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

typedef union
{
    struct
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };
    struct
    {
        uint8_t raw[3];
    };
} pixel_t;

typedef struct
{
    double h;
    double s;
    double v;
} hsv_t;

typedef struct
{
    uint8_t       command;
    pixel_t       pixel;
    uint8_t       maxTimeCount;
    pixel_t       last;
    uint8_t       timeCounter;
    uint8_t       offset;
    uint16_t      led;
    hsv_t         hsv;
    iterate_fp_t  fpIterate;
    uint8_t       buffer[16*3];
} leds_t;

//-------------------------------------------------------------------------------------------------

static QueueHandle_t gLedQueue = {0};
static leds_t        gLeds     = {0};

//-------------------------------------------------------------------------------------------------

static void led_RGBtoHSV(pixel_t * p_pixel, hsv_t * p_hsv)
{
    double min, max, delta;

    min = (p_pixel->r < p_pixel->g) ? p_pixel->r : p_pixel->g;
    min = (min < p_pixel->b) ? min : p_pixel->b;

    max = (p_pixel->r > p_pixel->g) ? p_pixel->r : p_pixel->g;
    max = (max > p_pixel->b) ? max : p_pixel->b;

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
    if (p_pixel->r == max)
    {
        p_hsv->h = (p_pixel->g - p_pixel->b) / delta;
    }
    else if (p_pixel->g == max)
    {
        p_hsv->h = 2 + (p_pixel->b - p_pixel->r) / delta;
    }
    else
    {
        p_hsv->h = 4 + (p_pixel->r - p_pixel->g) / delta;
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

static void led_HSVtoRGB(hsv_t * p_hsv, pixel_t * p_pixel)
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

    p_pixel->r = (uint8_t)(r * 255);
    p_pixel->g = (uint8_t)(g * 255);
    p_pixel->b = (uint8_t)(b * 255);
}

//-------------------------------------------------------------------------------------------------

// Performs linear interpolation between two values
static double led_LinearInterpolation(double a, double b, double t)
{
    return a + (b - a) * t;
}

//-------------------------------------------------------------------------------------------------

// Performs smooth color transition between two RGB colors
static void led_SmoothColorTransition(pixel_t * p_a, pixel_t * p_b, double prgs, pixel_t * p_r)
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
    pixel_t result = {0};

    if (100 > gLeds.offset)
    {
        led_SmoothColorTransition(&gLeds.last, &gLeds.pixel, gLeds.offset * 0.01, &result);
        LED_Strip_SetColor(result.r, result.g, result.b);
        gLeds.offset++;
    }
    else
    {
        LED_Strip_SetColor(gLeds.pixel.r, gLeds.pixel.g, gLeds.pixel.b);
        gLeds.last.r = gLeds.pixel.r;
        gLeds.last.g = gLeds.pixel.g;
        gLeds.last.b = gLeds.pixel.b;
        gLeds.command = LED_CMD_EMPTY;
    }
    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_Color(void)
{
    gLeds.offset       = 0;
    gLeds.maxTimeCount = 3;
    gLeds.timeCounter  = gLeds.maxTimeCount;

    LED_Strip_GetAverageColor(&gLeds.last.r, &gLeds.last.g, &gLeds.last.b);

    gLeds.fpIterate = led_IterateIndication_Color;
    gLeds.fpIterate();
}

//-------------------------------------------------------------------------------------------------
//--- Running LED Indication ----------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_Run(void)
{
    LED_Strip_Update();
    LED_Strip_Rotate(false);
    if (UINT16_MAX != gLeds.led)
    {
        gLeds.led = ((gLeds.led + 1) % (sizeof(gLeds.buffer) / 3)); 
        /* Switch the color R -> G -> B */
        if (0 == gLeds.led)
        {
            gLeds.pixel.raw[gLeds.offset++] = 0;
            gLeds.offset %= 3;
            gLeds.pixel.raw[gLeds.offset] = UINT8_MAX;
            LED_Strip_SetPixelColor(gLeds.led, gLeds.pixel.r, gLeds.pixel.g, gLeds.pixel.b);
        }
    }
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_Run(void)
{
    LED_Strip_Clear();

    /* Set the color depending on color settings */
    if (0 == (gLeds.pixel.r || gLeds.pixel.g || gLeds.pixel.b))
    {
        gLeds.offset  = 0;
        gLeds.led     = 0;
        gLeds.pixel.r = UINT8_MAX;
        LED_Strip_SetPixelColor(gLeds.led, gLeds.pixel.r, gLeds.pixel.g, gLeds.pixel.b);
    }
    else
    {
        gLeds.offset  = UINT8_MAX;
        gLeds.led     = UINT16_MAX;
        LED_Strip_SetPixelColor(0, gLeds.pixel.r, gLeds.pixel.g, gLeds.pixel.b);
    }

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
    enum
    {
        MAX_FADE_LEVEL = 30,
    };

    led_HSVtoRGB(&gLeds.hsv, &gLeds.pixel);
    LED_Strip_SetColor(gLeds.pixel.r, gLeds.pixel.g, gLeds.pixel.b);
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

static void led_SetIndication_Fade(void)
{
    LED_Strip_Clear();
    led_RGBtoHSV(&gLeds.pixel, &gLeds.hsv);
    gLeds.hsv.v        = 0.0;
    gLeds.offset       = 0;
    gLeds.led          = 0;
    gLeds.maxTimeCount = 2;
    gLeds.timeCounter  = gLeds.maxTimeCount;
    gLeds.fpIterate    = led_IterateIndication_Fade;
    gLeds.fpIterate();
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

static void led_SetIndication_PingPong(void)
{
    LED_Strip_Clear();
    gLeds.offset       = 0;
    gLeds.led          = 0;
    LED_Strip_SetPixelColor(gLeds.led, gLeds.pixel.r, gLeds.pixel.g, gLeds.pixel.b);
    gLeds.maxTimeCount = 3;
    gLeds.timeCounter  = gLeds.maxTimeCount;
    gLeds.fpIterate    = led_IterateIndication_PingPong;
    gLeds.fpIterate();
}

//-------------------------------------------------------------------------------------------------
//--- Rainbow LED Indication ---------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static void led_IterateIndication_Rainbow(void)
{
    LED_Strip_Rotate(false);
    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_SetIndication_Rainbow(void)
{
    double max = 0.0;

    if (0 == (gLeds.pixel.r || gLeds.pixel.g || gLeds.pixel.b))
    {
        /* Running Rainbow */
        max = 0.222;
        /* Set iteration period and callback */
        gLeds.maxTimeCount = 5;
        gLeds.timeCounter  = gLeds.maxTimeCount;
        gLeds.fpIterate    = led_IterateIndication_Rainbow;
    }
    else
    {
        /* Static Rainbow */
        max = (gLeds.pixel.r > gLeds.pixel.g) ? gLeds.pixel.r : gLeds.pixel.g;
        max = (max > gLeds.pixel.b) ? max : gLeds.pixel.b;
        max /= 255.0;
        /* Disable iteration */
        gLeds.command      = LED_CMD_EMPTY;
        gLeds.maxTimeCount = 0;
        gLeds.timeCounter  = 0;
        gLeds.fpIterate    = NULL;
    }

    /* Draw the Rainbow */
    gLeds.hsv.s = 1.0;
    gLeds.hsv.v = max;
    for (gLeds.led = 0; gLeds.led < (sizeof(gLeds.buffer) / 3); gLeds.led++)
    {
        gLeds.hsv.h = (gLeds.led * 1.0 / (sizeof(gLeds.buffer) / 3));
        led_HSVtoRGB(&gLeds.hsv, &gLeds.pixel);
        LED_Strip_SetPixelColor(gLeds.led, gLeds.pixel.r, gLeds.pixel.g, gLeds.pixel.b);
    }
    LED_Strip_Update();
}

//-------------------------------------------------------------------------------------------------

static void led_ProcessMsg(led_message_t * p_msg)
{
    gLeds.command = p_msg->command;
    gLeds.pixel.r = p_msg->red;
    gLeds.pixel.g = p_msg->green;
    gLeds.pixel.b = p_msg->blue;
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
        case LED_CMD_INDICATE_PINGPONG:
            led_SetIndication_PingPong();
            break;
        case LED_CMD_INDICATE_RAINBOW:
            led_SetIndication_Rainbow();
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
