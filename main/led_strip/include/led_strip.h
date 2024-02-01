#ifndef __LED_STRIP_H__
#define __LED_STRIP_H__

#include <stdint.h>

void LED_Strip_Init(uint8_t * leds, uint8_t count);
void LED_Strip_Update(void);
void LED_Strip_SetPixelColor(uint16_t pixel, uint8_t r, uint8_t g, uint8_t b);
void LED_Strip_Rotate(bool direction);
void LED_Strip_Clear(void);
void LED_Strip_SetColor(uint8_t r, uint8_t g, uint8_t b);

#endif /* __LED_STRIP_H__ */
