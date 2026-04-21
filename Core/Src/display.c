#include "display.h"
#include "fonts.h"
#include <string.h>

static SPI_HandleTypeDef *g_spi = NULL;
static uint16_t g_width = 240;
static uint16_t g_height = 320;

static inline void tft_select(void) {
    HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET);
}

static inline void tft_unselect(void) {
    HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET);
}

static inline void tft_dc_command(void) {
    HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_RESET);
}

static inline void tft_dc_data(void) {
    HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET);
}

static void write_command(uint8_t cmd) {
    tft_dc_command();
    tft_select();
    HAL_SPI_Transmit(g_spi, &cmd, 1, HAL_MAX_DELAY);
    tft_unselect();
}

static void write_data(const uint8_t *data, uint16_t size) {
    tft_dc_data();
    tft_select();
    HAL_SPI_Transmit(g_spi, (uint8_t *)data, size, HAL_MAX_DELAY);
    tft_unselect();
}

static void write_data8(uint8_t data) {
    write_data(&data, 1);
}

static void set_addr_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    uint8_t data[4];

    write_command(ILI9341_CASET);
    data[0] = (uint8_t)(x1 >> 8);
    data[1] = (uint8_t)(x1 & 0xFF);
    data[2] = (uint8_t)(x2 >> 8);
    data[3] = (uint8_t)(x2 & 0xFF);
    write_data(data, 4);

    write_command(ILI9341_PASET);
    data[0] = (uint8_t)(y1 >> 8);
    data[1] = (uint8_t)(y1 & 0xFF);
    data[2] = (uint8_t)(y2 >> 8);
    data[3] = (uint8_t)(y2 & 0xFF);
    write_data(data, 4);

    write_command(ILI9341_RAMWR);
}

void ILI9341_SetRotation(uint8_t rotation) {
    uint8_t madctl = 0x48;  // portrait default
    rotation &= 3;

    switch (rotation) {
        case 0:
            madctl = 0x48;
            g_width = 240;
            g_height = 320;
            break;
        case 1:
            madctl = 0x28;  // landscape
            g_width = 320;
            g_height = 240;
            break;
        case 2:
            madctl = 0x88;
            g_width = 240;
            g_height = 320;
            break;
        case 3:
            madctl = 0xE8;  // landscape flipped
            g_width = 320;
            g_height = 240;
            break;
    }

    write_command(ILI9341_MADCTL);
    write_data8(madctl);
}

void ILI9341_Init(SPI_HandleTypeDef *spi) {
    g_spi = spi;

    HAL_Delay(50);
    write_command(ILI9341_SWRESET);
    HAL_Delay(150);

    write_command(0xEF);
    write_data8(0x03);
    write_data8(0x80);
    write_data8(0x02);

    write_command(0xCF);
    write_data8(0x00);
    write_data8(0xC1);
    write_data8(0x30);

    write_command(ILI9341_PIXFMT);
    write_data8(0x55);  // 16-bit color

    ILI9341_SetRotation(1); // landscape by default

    write_command(ILI9341_SLPOUT);
    HAL_Delay(120);
    write_command(ILI9341_DISPON);
    HAL_Delay(20);
}

uint16_t ILI9341_Width(void) {
    return g_width;
}

uint16_t ILI9341_Height(void) {
    return g_height;
}

void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    uint8_t data[2];

    if (x >= g_width || y >= g_height) return;

    set_addr_window(x, y, x, y);
    data[0] = (uint8_t)(color >> 8);
    data[1] = (uint8_t)(color & 0xFF);
    write_data(data, 2);
}

void ILI9341_FillScreen(uint16_t color) {
    ILI9341_FillRect(0, 0, g_width, g_height, color);
}

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    static uint8_t block[1024];
    uint8_t hi, lo;
    uint32_t bytes;
    uint16_t chunk;
    uint16_t i;

    if (x >= g_width || y >= g_height) return;
    if ((x + w) > g_width)  w = g_width - x;
    if ((y + h) > g_height) h = g_height - y;
    if (w == 0 || h == 0) return;

    hi = (uint8_t)(color >> 8);
    lo = (uint8_t)(color & 0xFF);
    for (i = 0; i < sizeof(block); i += 2) {
        block[i] = hi;
        block[i + 1] = lo;
    }

    set_addr_window(x, y, x + w - 1, y + h - 1);
    tft_dc_data();
    tft_select();

    bytes = (uint32_t)w * h * 2U;
    while (bytes > 0) {
        chunk = (bytes > sizeof(block)) ? sizeof(block) : (uint16_t)bytes;
        HAL_SPI_Transmit(g_spi, block, chunk, HAL_MAX_DELAY);
        bytes -= chunk;
    }

    tft_unselect();
}

void ILI9341_DrawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    ILI9341_FillRect(x, y, w, 1, color);
}

void ILI9341_DrawFastVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color) {
    ILI9341_FillRect(x, y, 1, h, color);
}

void ILI9341_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (w == 0 || h == 0) return;
    ILI9341_DrawFastHLine(x, y, w, color);
    ILI9341_DrawFastHLine(x, y + h - 1, w, color);
    ILI9341_DrawFastVLine(x, y, h, color);
    ILI9341_DrawFastVLine(x + w - 1, y, h, color);
}

static void draw_char_gfx(int16_t x, int16_t y, unsigned char c, uint16_t color) {
    if (!gfxFont) return;
    if (c < gfxFont->first || c > gfxFont->last) return;

    GFXglyph *glyph = &gfxFont->glyph[c - gfxFont->first];
    uint8_t *bitmap = gfxFont->bitmap;
    uint16_t bo = glyph->bitmapOffset;
    uint8_t w = glyph->width;
    uint8_t h = glyph->height;
    int8_t xo = glyph->xOffset;
    int8_t yo = glyph->yOffset;

    uint8_t bits = 0, bit = 0;

    for (uint8_t yy = 0; yy < h; yy++) {
        for (uint8_t xx = 0; xx < w; xx++) {
            if (!(bit++ & 7)) {
                bits = bitmap[bo++];
            }
            if (bits & 0x80) {
                ILI9341_DrawPixel(x + xo + xx, y + yo + yy, color);
            }
            bits <<= 1;
        }
    }
}

void ILI9341_DrawText(int16_t x, int16_t y, const char *str, const GFXfont *font, uint16_t color) {
    if (!str || !font) return;

    gfxFont = (GFXfont *)font;

    while (*str) {
        unsigned char c = (unsigned char)*str;

        if (c == '\n') {
            y += gfxFont->yAdvance;
            x = 0;
        } else if (c >= gfxFont->first && c <= gfxFont->last) {
            GFXglyph *glyph = &gfxFont->glyph[c - gfxFont->first];
            draw_char_gfx(x, y, c, color);
            x += glyph->xAdvance;
        }

        str++;
    }
}
