#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_attr.h"
#include "gpio.h"
#include "esp_clk.h"

#include "esp8266/uart_struct.h"
#include "esp8266/uart_register.h"
#include "esp8266/pin_mux_register.h"
#include "esp8266/eagle_soc.h"
#include "esp8266/rom_functions.h"
#include "rom/ets_sys.h"
#include "driver/uart.h"
#include "driver/uart_select.h"


#define LEDS_ENTER_CRITICAL()      portENTER_CRITICAL()
#define LEDS_EXIT_CRITICAL()       portEXIT_CRITICAL()
/* 800kHz, 4 serial bytes per NeoByte */
#define LEDS_UART_BAUDRATE              (3200000)
#define LEDS_UART_EMPTY_THRESH_DEFAULT  (80)
#define LEDS_EVT_COMPLETE               (BIT0)

/* DRAM_ATTR is required to avoid UART array placed in flash,
   due to accessed from ISR */
static DRAM_ATTR uart_dev_t * const gUART        = &uart0;
static uint8_t *                    gLeds        = NULL;
static uint8_t                      gLedsCount   = 0;
static EventGroupHandle_t           gLedsEvents  = NULL;

static uint8_t *                    gStart       = NULL;
static uint8_t *                    gEnd         = NULL;

//-------------------------------------------------------------------------------------------------

static EventBits_t leds_WaitFor(EventBits_t events, TickType_t timeout)
{
    EventBits_t bits = 0;

    /* Waiting until either specified event is set */
    bits = xEventGroupWaitBits
           (
               gLedsEvents,
               events,       /* Bits To Wait For */
               pdTRUE,       /* Clear On Exit */
               pdFALSE,      /* Wait For All Bits */
               timeout / portTICK_RATE_MS
           );

    return bits;
}

//-------------------------------------------------------------------------------------------------

uint8_t * IRAM_ATTR ledstrip_FillUartFifo(uint8_t * leds, uint8_t * end)
{
    // Remember: UARTs send less significant bit (LSB) first so
    //      pushing ABCDEF byte will generate a 0FEDCBA1 signal,
    //      including a LOW(0) start & a HIGH(1) stop bits.
    // Also, we have configured UART to invert logic levels, so:
    const uint8_t _uartData[4] =
    {
        0b110111, // On wire: 1 000 100 0 [Neopixel reads 00]
        0b000111, // On wire: 1 000 111 0 [Neopixel reads 01]
        0b110100, // On wire: 1 110 100 0 [Neopixel reads 10]
        0b000100, // On wire: 1 110 111 0 [NeoPixel reads 11]
    };
    uint8_t avail = (UART_FIFO_LEN - gUART->status.txfifo_cnt) / 4;
    if (end - leds > avail)
    {
        end = leds + avail;
    }
    while (leds < end)
    {
        uint8_t subpix = *leds++;
        gUART->fifo.rw_byte = _uartData[(subpix >> 6) & 0x3];
        gUART->fifo.rw_byte = _uartData[(subpix >> 4) & 0x3];
        gUART->fifo.rw_byte = _uartData[(subpix >> 2) & 0x3];
        gUART->fifo.rw_byte = _uartData[ subpix       & 0x3];
    }
    return leds;
}

//-------------------------------------------------------------------------------------------------

static void ledstrip_UartIrqHandler(void * param)
{
    uint32_t uart_intr_status = gUART->int_st.val;
    BaseType_t xHigherPriorityTaskWoken, xResult;

    while (0 != uart_intr_status)
    {
        if ( 0 != (uart_intr_status & UART_TXFIFO_EMPTY_INT_ST_M) )
        {
            /* Clear the interrupt flag */
            gUART->int_clr.txfifo_empty = 1;

            /* Fill the FIFO with new data */
            gStart = ledstrip_FillUartFifo(gStart, gEnd);

            /* Disable TX interrupt when done */
            if (gStart == gEnd)
            {
                gUART->int_ena.txfifo_empty = 0;
                /* Set event */
                /* xHigherPriorityTaskWoken must be initialised to pdFALSE */
                xHigherPriorityTaskWoken = pdFALSE;
                /* Set bits in EventGroup */
                xResult = xEventGroupSetBitsFromISR
                          (
                              gLedsEvents,
                              LEDS_EVT_COMPLETE,
                              &xHigherPriorityTaskWoken
                          );
                /* Was the event set successfully? */
                if ((pdPASS == xResult) && (pdTRUE == xHigherPriorityTaskWoken))
                {
                    /* If xHigherPriorityTaskWoken is now set to pdTRUE then a context
                     * switch should be requested. The macro used is port specific and
                     * will be either portYIELD_FROM_ISR() or portEND_SWITCHING_ISR() -
                     * refer to the documentation page for the port being used. */
                    portYIELD_FROM_ISR(); //xHigherPriorityTaskWoken);
                }
            }
        }
        else
        {
            /* Simply clear all the other IRQ flags */
            gUART->int_clr.val = UART_INTR_MASK; //uart_intr_status;
        }

        /* Get the IRQ flags */
        uart_intr_status = gUART->int_st.val;
    }
}

//-------------------------------------------------------------------------------------------------

static void ledstrip_InitializeUart(void)
{
    /* Create the events group for UDP task */
    gLedsEvents = xEventGroupCreate();

    /* Init the GPIO pins of UART */
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);

    /* Disable all interrupts */
    LEDS_ENTER_CRITICAL();

    /* Disable the hardware flow control */
    gUART->conf1.rx_flow_en = 0;
    gUART->conf0.tx_flow_en = 0;

    /* Set the baud rate */
    gUART->clk_div.val = (uint32_t)(UART_CLK_FREQ / LEDS_UART_BAUDRATE) & 0xFFFFF;

    /* Set the word length */        
    gUART->conf0.bit_num = UART_DATA_6_BITS;

    /* Set the stop bits */
    gUART->conf0.stop_bit_num = UART_STOP_BITS_1;

    /* Set the parity */
    gUART->conf0.parity = (UART_PARITY_DISABLE & 0x1);
    gUART->conf0.parity_en = ((UART_PARITY_DISABLE >> 1) & 0x1);

    /* Reset the Rx FIFO */
    gUART->conf0.rxfifo_rst = 0x1;
    gUART->conf0.rxfifo_rst = 0x0;
    gUART->conf0.txfifo_rst = 0x1;
    gUART->conf0.txfifo_rst = 0x0;

    /* Set the Rx/Tx/Timeout treshholds */
    gUART->conf1.txfifo_empty_thrhd = LEDS_UART_EMPTY_THRESH_DEFAULT;

    /* Invert the TX voltage associated with logic level so:
     *    - A logic level 0 will generate a Vcc signal
     *    - A logic level 1 will generate a Gnd signal */
    gUART->conf0.txd_inv = 1;

    LEDS_EXIT_CRITICAL();

    /* Register the IRQ handler */
    (void)uart_isr_register(UART_NUM_0, ledstrip_UartIrqHandler, NULL);

    LEDS_ENTER_CRITICAL();

    /* Clear the interrupts' flags */
    gUART->int_clr.val = UART_INTR_MASK;

    LEDS_EXIT_CRITICAL();
}

//-------------------------------------------------------------------------------------------------

static void ledstrip_UpdateUart(void)
{
    gStart = gLeds;
    gEnd   = gLeds + gLedsCount;

    vTaskDelay(10 / portTICK_RATE_MS);

    LEDS_ENTER_CRITICAL();

    /* Clear the interrupt flag */
    gUART->int_clr.txfifo_empty = 1;
    /* Enable the interrupt */
    gUART->int_ena.txfifo_empty = 1;

    LEDS_EXIT_CRITICAL();

    (void)leds_WaitFor(LEDS_EVT_COMPLETE, portMAX_DELAY);
}

//-------------------------------------------------------------------------------------------------

void LED_Strip_Init(uint8_t * leds, uint8_t count)
{
    gLeds      = leds;
    gLedsCount = count;

    /* Clear all the pixels */
    memset(gLeds, 0, gLedsCount);

    ledstrip_InitializeUart();

    ledstrip_UpdateUart();
}

//-------------------------------------------------------------------------------------------------

void LED_Strip_Update(void)
{
    ledstrip_UpdateUart();
}

//-------------------------------------------------------------------------------------------------

void LED_Strip_SetPixelColor(uint16_t pixel, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t pos = pixel * 3;

    if (gLedsCount <= (pos + 2)) return;

    gLeds[pos++] = g;
    gLeds[pos++] = r;
    gLeds[pos++] = b;
}

//-------------------------------------------------------------------------------------------------

void LED_Strip_Rotate(bool direction)
{
    uint8_t led[3];

    if (true == direction)
    {
        memcpy(led, gLeds, 3);
        memmove(gLeds, gLeds + 3, gLedsCount - 3);
        memcpy(gLeds + gLedsCount - 3, led, 3);
    }
    else
    {
        memcpy(led, gLeds + gLedsCount - 3, 3);
        memmove(gLeds + 3, gLeds, gLedsCount - 3);
        memcpy(gLeds, led, 3);
    }
}

//-------------------------------------------------------------------------------------------------

void LED_Strip_Clear(void)
{
    memset(gLeds, 0, gLedsCount);
}

//-------------------------------------------------------------------------------------------------

void LED_Strip_SetColor(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t pos = 0;

    for (pos = 0; pos < gLedsCount;)
    {
        gLeds[pos++] = g;
        gLeds[pos++] = r;
        gLeds[pos++] = b;
    }
}

//-------------------------------------------------------------------------------------------------

void LED_Strip_GetAverageColor(uint8_t * p_r, uint8_t * p_g, uint8_t * p_b)
{
    uint16_t r = 0, g = 0, b = 0;
    uint32_t pos = 0;

    for (pos = 0; pos < gLedsCount;)
    {
        g += gLeds[pos++];
        r += gLeds[pos++];
        b += gLeds[pos++];
    }

    pos = (gLedsCount / 3);

    *p_r = (uint8_t)(r / pos);
    *p_g = (uint8_t)(g / pos);
    *p_b = (uint8_t)(b / pos);
}

//-------------------------------------------------------------------------------------------------
