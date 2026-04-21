#ifndef DISPLAY_H
#define DISPLAY_H

#include "main.h"
#include "fonts.h"
#include <stdint.h>

#define TFT_CS_PORT   GPIOB
#define TFT_CS_PIN    GPIO_PIN_6
#define TFT_DC_PORT   GPIOC
#define TFT_DC_PIN    GPIO_PIN_7

#define ILI9341_SWRESET  0x01
#define ILI9341_SLPOUT   0x11
#define ILI9341_DISPON   0x29
#define ILI9341_CASET    0x2A
#define ILI9341_PASET    0x2B
#define ILI9341_RAMWR    0x2C
#define ILI9341_MADCTL   0x36
#define ILI9341_PIXFMT   0x3A

#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_BLUE    0x001F
#define TFT_YELLOW  0xFFE0
#define TFT_CYAN    0x07FF
#define TFT_MAGENTA 0xF81F
#define TFT_GRAY    0x8410
#define TFT_ORANGE  0xFD20

void ILI9341_Init(SPI_HandleTypeDef *spi);
void ILI9341_SetRotation(uint8_t rotation);
uint16_t ILI9341_Width(void);
uint16_t ILI9341_Height(void);

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_DrawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void ILI9341_DrawFastVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void ILI9341_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_DrawText(int16_t x, int16_t y, const char *str, const GFXfont *font, uint16_t color);
#endif
