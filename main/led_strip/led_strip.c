#include <stdint.h>
#include <string.h>
#include "esp_attr.h"
#include "gpio.h"
#include "esp_clk.h"

static uint8_t * gPixels      = NULL;
static uint8_t   gPixelsCount = 0;

inline uint32_t _getCycleCount()
{
    uint32_t ccount;
    __asm__ __volatile__("rsr %0, ccount":"=a" (ccount));
    return ccount;
}

#define CYCLES_800_T0H  (esp_clk_cpu_freq() / 2500000) // 0.4us
#define CYCLES_800_T1H  (esp_clk_cpu_freq() / 1250000) // 0.8us
#define CYCLES_800      (esp_clk_cpu_freq() /  800000) // 1.25us per bit
//#define CYCLES_400_T0H  (F_CPU / 2000000)
//#define CYCLES_400_T1H  (F_CPU /  833333)
//#define CYCLES_400      (F_CPU /  400000) 

static void IRAM_ATTR bitbang_send_pixels_800(uint8_t * pixels, uint8_t count, uint8_t pin)
{
    const uint32_t pinRegister = BIT(pin);
    uint8_t        mask        = 0;
    uint8_t        subpix      = 0;
    uint32_t       cyclesStart = 0;
    uint8_t *      end         = (pixels + count);

    uint32_t       cycles_lo   = CYCLES_800;
    uint32_t       cycles_1_hi = CYCLES_800_T1H;
    uint32_t       cycles_0_hi = CYCLES_800_T0H;

    /* Trigger emediately */
    cyclesStart = _getCycleCount() - cycles_lo; //CYCLES_800;
    do
    {
        subpix = *pixels++;
        for (mask = 0x80; mask != 0; mask >>= 1)
        {
            /* Do the checks here while we are waiting on time to pass */
            uint32_t cyclesBit = ((subpix & mask)) ? cycles_1_hi : cycles_0_hi; //CYCLES_800_T1H : CYCLES_800_T0H;
            uint32_t cyclesNext = cyclesStart;

            /* After we have done as much work as needed for this next bit */
            /* now wait for the HIGH */
            do
            {
                /* Cache and use this count so we don't incur another */
                /* instruction before we turn the bit high */
                cyclesStart = _getCycleCount();
            }
            while ((cyclesStart - cyclesNext) < cycles_lo); //CYCLES_800);

            /* Set high */
            GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinRegister);

            /* Wait for the LOW */
            do
            {
                cyclesNext = _getCycleCount();
            }
            while ((cyclesNext - cyclesStart) < cyclesBit);

            /* Set low */
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinRegister);
        }
    }
    while (pixels < end);
}

void LED_Strip_Init(uint8_t * pixels, uint8_t count)
{
    gpio_config_t io_conf   = {0};

    gPixels      = pixels;
    gPixelsCount = count;

    /* Clear all the pixels */
    memset(gPixels, 0, gPixelsCount);

    /* Disable interrupt */
    io_conf.intr_type = GPIO_INTR_DISABLE;
    /* Set as output mode */
    io_conf.mode = GPIO_MODE_OUTPUT;
    /* Bit mask of the pins that you want to set */
    io_conf.pin_bit_mask = BIT(GPIO_NUM_1);
    /* Disable pull-down mode */
    io_conf.pull_down_en = 0;
    /* Disable pull-up mode */
    io_conf.pull_up_en = 0;
    /* Configure GPIO with the given settings */
    gpio_config(&io_conf);
    
    bitbang_send_pixels_800(gPixels, gPixelsCount, GPIO_NUM_1);
}

void LED_Strip_Update(void)
{
    bitbang_send_pixels_800(gPixels, gPixelsCount, GPIO_NUM_1);
}

/*
void ICACHE_RAM_ATTR bitbang_send_pixels_400(uint8_t* pixels, uint8_t* end, uint8_t pin)
{
    const uint32_t pinRegister = _BV(pin);
    uint8_t mask;
    uint8_t subpix;
    uint32_t cyclesStart;

    // trigger emediately
    cyclesStart = _getCycleCount() - CYCLES_400;
    do
    {
        subpix = *pixels++;
        for (mask = 0x80; mask; mask >>= 1)
        {
            uint32_t cyclesBit = ((subpix & mask)) ? CYCLES_400_T1H : CYCLES_400_T0H;
            uint32_t cyclesNext = cyclesStart;

            // after we have done as much work as needed for this next bit
            // now wait for the HIGH
            do
            {
                // cache and use this count so we don't incur another 
                // instruction before we turn the bit high
                cyclesStart = _getCycleCount();
            } while ((cyclesStart - cyclesNext) < CYCLES_400);

#if defined(ARDUINO_ARCH_ESP32)
            GPIO.out_w1ts = pinRegister;
#else
            GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, pinRegister);
#endif

            // wait for the LOW
            do
            {
                cyclesNext = _getCycleCount();
            } while ((cyclesNext - cyclesStart) < cyclesBit);

            // set low
#if defined(ARDUINO_ARCH_ESP32)
            GPIO.out_w1tc = pinRegister;
#else
            GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, pinRegister);
#endif
        }
    } while (pixels < end);
}
*/