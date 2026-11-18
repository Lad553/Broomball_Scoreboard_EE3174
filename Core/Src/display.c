// Includes
#include "display.h"
#include "fonts.h"
#include <string.h>

// Global variables
static SPI_HandleTypeDef *g_spi = NULL;
static uint16_t g_width = 240;
static uint16_t g_height = 320;

// Pulls chip-select low to begin an SPI transaction
static inline void tft_select(void) {
	HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET);
}

// Pulls chip-select high to end an SPI transaction
static inline void tft_unselect(void) {
	HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET);
}

// Pulls D/C low to signal a command byte
static inline void tft_dc_command(void) {
	HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_RESET);
}

// Pulls D/C high to signal data bytes
static inline void tft_dc_data(void) {
	HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET);
}

/*
Takes in a command byte
Sends a single command to the display over SPI
Does not return anything
*/
static void write_command(uint8_t cmd) {
	tft_dc_command();
	tft_select();
	HAL_SPI_Transmit(g_spi, &cmd, 1, HAL_MAX_DELAY);
	tft_unselect();
}

/*
Takes in a pointer to data bytes and the number of bytes to send
Sends raw data to the display over SPI
Does not return anything
*/
static void write_data(const uint8_t *data, uint16_t size) {
	tft_dc_data();
	tft_select();
	HAL_SPI_Transmit(g_spi, (uint8_t *)data, size, HAL_MAX_DELAY);
	tft_unselect();
}

/*
Takes in a single data byte
Sends one byte of data to the display over SPI
Does not return anything
*/
static void write_data8(uint8_t data) {
	write_data(&data, 1);
}

/*
Takes in a command byte
Sends command with CS already asserted then leaves DC in data mode for pixel streaming
Does not return anything
*/
static inline void raw_cmd(uint8_t cmd) {
	tft_dc_command();
	HAL_SPI_Transmit(g_spi, &cmd, 1, HAL_MAX_DELAY);
	tft_dc_data();
}

/*
Takes in the top-left (x1, y1) and bottom-right (x2, y2) pixel coordinates
Sets the ILI9341 address window and issues RAMWR; asserts CS once for the whole sequence
Leaves CS asserted and DC in data mode so the caller can stream pixel data immediately
Does not return anything
*/
static void set_addr_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
	uint8_t data[4];

	tft_select();

	raw_cmd(ILI9341_CASET);
	data[0] = (uint8_t)(x1 >> 8); data[1] = (uint8_t)x1;
	data[2] = (uint8_t)(x2 >> 8); data[3] = (uint8_t)x2;
	HAL_SPI_Transmit(g_spi, data, 4, HAL_MAX_DELAY);

	raw_cmd(ILI9341_PASET);
	data[0] = (uint8_t)(y1 >> 8); data[1] = (uint8_t)y1;
	data[2] = (uint8_t)(y2 >> 8); data[3] = (uint8_t)y2;
	HAL_SPI_Transmit(g_spi, data, 4, HAL_MAX_DELAY);

	raw_cmd(ILI9341_RAMWR);
	// CS stays asserted; DC is in data mode. Caller writes pixel bytes, then calls tft_unselect()
}

/*
Takes in a rotation value (0-3)
Sets the display orientation and updates g_width/g_height accordingly
Does not return anything
*/
void ILI9341_SetRotation(uint8_t rotation) {
	uint8_t madctl = 0x48;
	rotation &= 3;

	switch (rotation) {
		case 0: madctl = 0x48; g_width = 240; g_height = 320; break;
		case 1: madctl = 0x28; g_width = 320; g_height = 240; break;
		case 2: madctl = 0x88; g_width = 240; g_height = 320; break;
		case 3: madctl = 0xE8; g_width = 320; g_height = 240; break;
	}

	write_command(ILI9341_MADCTL);
	write_data8(madctl);
}

/*
Takes in a pointer to the SPI handle
Resets, configures, and wakes the ILI9341; sets landscape orientation by default
Does not return anything
*/
void ILI9341_Init(SPI_HandleTypeDef *spi) {
	g_spi = spi;

	HAL_Delay(50);
	write_command(ILI9341_SWRESET);
	HAL_Delay(150);

	// Extra display register setup
	write_command(0xEF);
	write_data8(0x03);
	write_data8(0x80);
	write_data8(0x02);

	write_command(0xCF);
	write_data8(0x00);
	write_data8(0xC1);
	write_data8(0x30);

	// Use 16 bit color
	write_command(ILI9341_PIXFMT);
	write_data8(0x55);

	ILI9341_SetRotation(1);

	write_command(ILI9341_SLPOUT);
	HAL_Delay(120);
	write_command(ILI9341_DISPON);
	HAL_Delay(20);
}

// Returns the display width in pixels for the current rotation
uint16_t ILI9341_Width(void)  {
	return g_width;
}

// Returns the display height in pixels for the current rotation
uint16_t ILI9341_Height(void) {
	return g_height;
}

/*
Takes in pixel coordinates (x, y) and a 16-bit RGB565 color
Draws a single pixel at the given position
Does not return anything
*/
void ILI9341_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
	uint8_t data[2];

	if (x >= g_width || y >= g_height) return;

	set_addr_window(x, y, x, y);
	data[0] = (uint8_t)(color >> 8);
	data[1] = (uint8_t)(color & 0xFF);
	HAL_SPI_Transmit(g_spi, data, 2, HAL_MAX_DELAY);
	tft_unselect();
}

/*
Takes in top-left coordinates (x, y), dimensions (w, h), and a 16-bit RGB565 color
Fills a rectangle with the given color; caches the pixel block to skip re-initialization when the color hasn't changed
Does not return anything
*/
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
	static uint8_t  block[1024];
	static uint16_t blockColor = 0x0001; // impossible initial value forces first-call init
	static uint8_t  blockInited = 0;
	uint32_t bytes;
	uint16_t chunk;

	if (x >= g_width || y >= g_height) return;
	if ((x + w) > g_width)  w = g_width  - x;
	if ((y + h) > g_height) h = g_height - y;
	if (w == 0 || h == 0) return;

	// Only rebuild this block when the color changes
	if (!blockInited || color != blockColor) {
		uint8_t hi = (uint8_t)(color >> 8), lo = (uint8_t)(color);
		for (uint16_t i = 0; i < sizeof(block); i += 2){
			block[i] = hi; block[i + 1] = lo;
		}
		blockColor = color;
		blockInited = 1;
	}

	set_addr_window(x, y, x + w - 1, y + h - 1);

	bytes = (uint32_t)w * h * 2U;
	while (bytes > 0) {
		chunk = (bytes > sizeof(block)) ? (uint16_t)sizeof(block) : (uint16_t)bytes;
		HAL_SPI_Transmit(g_spi, block, chunk, HAL_MAX_DELAY);
		bytes -= chunk;
	}

	tft_unselect();
}

/*
Takes in a 16-bit RGB565 color
Fills the entire display with that color
Does not return anything
*/
void ILI9341_FillScreen(uint16_t color) {
	ILI9341_FillRect(0, 0, g_width, g_height, color);
}

/*
Takes in start coordinates (x, y), a width w, and a 16-bit color
Draws a horizontal line
Does not return anything
*/
void ILI9341_DrawFastHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
	ILI9341_FillRect(x, y, w, 1, color);
}

/*
Takes in start coordinates (x, y), a height h, and a 16-bit color
Draws a vertical line
Does not return anything
*/
void ILI9341_DrawFastVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color) {
	ILI9341_FillRect(x, y, 1, h, color);
}

/*
Takes in top-left coordinates (x, y), dimensions (w, h), and a 16-bit color
Draws a hollow rectangle border
Does not return anything
*/
void ILI9341_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
	if (w == 0 || h == 0) return;
	ILI9341_DrawFastHLine(x, y, w, color);
	ILI9341_DrawFastHLine(x, y + h - 1, w, color);
	ILI9341_DrawFastVLine(x, y, h, color);
	ILI9341_DrawFastVLine(x + w - 1, y, h, color);
}

/*
Takes in screen position (x, y), a character c, and a foreground color
Renders one GFX-font glyph as a single SPI transaction using a pixel buffer
All text is rendered over a black background so unlit pixels are hardcoded TFT_BLACK
Does not return anything
*/
static void draw_char_gfx(int16_t x, int16_t y, unsigned char c, uint16_t color) {
	if (!gfxFont)
		return;
	if (c < gfxFont->first || c > gfxFont->last)
		return;

	GFXglyph *glyph = &gfxFont->glyph[c - gfxFont->first];
	uint8_t *bitmap = gfxFont->bitmap;
	uint8_t w = glyph->width, h = glyph->height;
	int8_t xo = glyph->xOffset, yo = glyph->yOffset;

	if (w == 0 || h == 0) return;

	// Pixel buffer for one character
	static uint8_t pbuf[512];

	uint8_t fg_hi = (uint8_t)(color >> 8), fg_lo = (uint8_t)(color);
	uint16_t bo = glyph->bitmapOffset;
	uint8_t bits = 0, bit = 0;
	uint16_t idx = 0;

	for (uint8_t yy = 0; yy < h; yy++) {
		for (uint8_t xx = 0; xx < w; xx++) {
			if (!(bit++ & 7)) bits = bitmap[bo++];
			if (bits & 0x80) {
				pbuf[idx++] = fg_hi;
				pbuf[idx++] = fg_lo;
			} else {
				pbuf[idx++] = 0x00; // TFT_BLACK high byte
				pbuf[idx++] = 0x00; // TFT_BLACK low byte
			}
			bits <<= 1;
		}
	}

	set_addr_window((uint16_t)(x + xo), (uint16_t)(y + yo), (uint16_t)(x + xo + w - 1), (uint16_t)(y + yo + h - 1));
	HAL_SPI_Transmit(g_spi, pbuf, idx, HAL_MAX_DELAY);
	tft_unselect();
}

/*
Takes in screen position (x, y), a null-terminated string, a GFX font pointer, and a foreground color
Draws each character of the string using the given font; advances x by each glyph's xAdvance
Does not return anything
*/
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
