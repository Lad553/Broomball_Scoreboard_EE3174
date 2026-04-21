#include "touch.h"

static I2C_HandleTypeDef *g_i2c = NULL;
static uint16_t g_tsc_addr = 0;
static bool g_touch_ready = false;

/*
 * Valid 7-bit TSC2007 addresses on the Adafruit shield:
 * 0x48, 0x49, 0x4A, 0x4B
 */
#define TSC2007_ADDR_MIN_7BIT  0x48U
#define TSC2007_ADDR_MAX_7BIT  0x4BU

/*
 * Correct TSC2007 command format:
 * [ C3 C2 C1 C0 | PD1 PD0 | M | X ]
 *
 * C3:C0  -> function select (bits 7:4)
 * PD1:PD0-> power-down mode   (bits 3:2)
 * M      -> resolution        (bit 1)   0 = 12-bit, 1 = 8-bit
 * X      -> don't care        (bit 0)
 */

/* Function select field (bits 7:4) */
#define TSC2007_FUNC_TEMP0      0x00U
#define TSC2007_FUNC_X          0xC0U
#define TSC2007_FUNC_Y          0xD0U
#define TSC2007_FUNC_Z1         0xE0U
#define TSC2007_FUNC_Z2         0xF0U

/* Power-down field (bits 3:2) */
#define TSC2007_PD_POWERDOWN_IRQ  0x00U  /* 00 << 2 */
#define TSC2007_PD_ADC_ON         0x04U  /* 01 << 2 */

/* Resolution field (bit 1) */
#define TSC2007_RES_12BIT         0x00U
#define TSC2007_RES_8BIT          0x02U

#define TSC2007_CMD(func, pd, res) ((uint8_t)((func) | (pd) | (res)))

#define TSC2007_MAX_READING 4095U
#define TOUCH_SAMPLE_TOLERANCE 80U

static uint16_t abs_diff_u16(uint16_t a, uint16_t b)
{
    return (a >= b) ? (a - b) : (b - a);
}

static int32_t clamp_map_i32(int32_t v,
                             int32_t in_min, int32_t in_max,
                             int32_t out_min, int32_t out_max)
{
    if (in_max <= in_min) {
        return out_min;
    }

    if (v < in_min) v = in_min;
    if (v > in_max) v = in_max;

    return ((v - in_min) * (out_max - out_min)) / (in_max - in_min) + out_min;
}

static bool tsc2007_read_command(uint8_t cmd, uint16_t *value)
{
    uint8_t rx[2] = {0, 0};

    if ((g_i2c == NULL) || (value == NULL) || !g_touch_ready) {
        return false;
    }

    /*
     * The TSC2007 takes a one-byte command and then returns 12-bit data.
     * HAL_I2C_Mem_Read works well here because it sends the command byte
     * first, then reads the response bytes.
     */
    if (HAL_I2C_Mem_Read(g_i2c,
                         g_tsc_addr,
                         cmd,
                         I2C_MEMADD_SIZE_8BIT,
                         rx,
                         2,
                         HAL_MAX_DELAY) != HAL_OK) {
        return false;
    }

    *value = (uint16_t)(((uint16_t)rx[0] << 4) | (rx[1] >> 4));
    return true;
}

static void tsc2007_enter_idle(void)
{
    uint16_t dummy;

    if (!g_touch_ready) {
        return;
    }

    (void)tsc2007_read_command(
        TSC2007_CMD(TSC2007_FUNC_TEMP0, TSC2007_PD_POWERDOWN_IRQ, TSC2007_RES_12BIT),
        &dummy
    );
}

void TSC2007_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t addr7;

    g_i2c = hi2c;
    g_tsc_addr = 0;
    g_touch_ready = false;

    if (g_i2c == NULL) {
        return;
    }

    /*
     * Only scan the valid TSC2007 address range instead of latching onto
     * some unrelated I2C device.
     */
    for (addr7 = TSC2007_ADDR_MIN_7BIT; addr7 <= TSC2007_ADDR_MAX_7BIT; addr7++) {
        if (HAL_I2C_IsDeviceReady(g_i2c, (uint16_t)(addr7 << 1), 2, 50) == HAL_OK) {
            g_tsc_addr = (uint16_t)(addr7 << 1);
            g_touch_ready = true;
            break;
        }
    }

    if (!g_touch_ready) {
        return;
    }

    tsc2007_enter_idle();
}

bool TSC2007_ReadRaw(uint16_t *x, uint16_t *y, uint16_t *z1, uint16_t *z2)
{
    uint16_t rz1 = 0;
    uint16_t rz2 = 0;
    uint16_t rx1 = 0, ry1 = 0;
    uint16_t rx2 = 0, ry2 = 0;

    if (!g_touch_ready) {
        return false;
    }

    /* Pressure check first */
    if (!tsc2007_read_command(
            TSC2007_CMD(TSC2007_FUNC_Z1, TSC2007_PD_ADC_ON, TSC2007_RES_12BIT),
            &rz1)) {
        tsc2007_enter_idle();
        return false;
    }

    if ((rz1 < TOUCH_Z_THRESHOLD) || (rz1 >= TSC2007_MAX_READING)) {
        tsc2007_enter_idle();
        return false;
    }

    if (!tsc2007_read_command(
            TSC2007_CMD(TSC2007_FUNC_Z2, TSC2007_PD_ADC_ON, TSC2007_RES_12BIT),
            &rz2)) {
        tsc2007_enter_idle();
        return false;
    }

    /*
     * Take two X/Y samples and require them to be reasonably close.
     * This helps reject noisy pen-up/edge transitions.
     */
    if (!tsc2007_read_command(
            TSC2007_CMD(TSC2007_FUNC_X, TSC2007_PD_ADC_ON, TSC2007_RES_12BIT),
            &rx1)) {
        tsc2007_enter_idle();
        return false;
    }

    if (!tsc2007_read_command(
            TSC2007_CMD(TSC2007_FUNC_Y, TSC2007_PD_ADC_ON, TSC2007_RES_12BIT),
            &ry1)) {
        tsc2007_enter_idle();
        return false;
    }

    if (!tsc2007_read_command(
            TSC2007_CMD(TSC2007_FUNC_X, TSC2007_PD_ADC_ON, TSC2007_RES_12BIT),
            &rx2)) {
        tsc2007_enter_idle();
        return false;
    }

    if (!tsc2007_read_command(
            TSC2007_CMD(TSC2007_FUNC_Y, TSC2007_PD_ADC_ON, TSC2007_RES_12BIT),
            &ry2)) {
        tsc2007_enter_idle();
        return false;
    }

    tsc2007_enter_idle();

    if ((rx1 == 0U) || (ry1 == 0U) || (rx2 == 0U) || (ry2 == 0U)) {
        return false;
    }

    if ((rx1 >= TSC2007_MAX_READING) || (ry1 >= TSC2007_MAX_READING) ||
        (rx2 >= TSC2007_MAX_READING) || (ry2 >= TSC2007_MAX_READING)) {
        return false;
    }

    if (abs_diff_u16(rx1, rx2) > TOUCH_SAMPLE_TOLERANCE) {
        return false;
    }

    if (abs_diff_u16(ry1, ry2) > TOUCH_SAMPLE_TOLERANCE) {
        return false;
    }

    if (x)  *x  = (uint16_t)((rx1 + rx2) / 2U);
    if (y)  *y  = (uint16_t)((ry1 + ry2) / 2U);
    if (z1) *z1 = rz1;
    if (z2) *z2 = rz2;

    return true;
}

bool Touch_ReadScreenPoint(TouchPoint *p)
{
    uint16_t raw_x, raw_y, raw_z1, raw_z2;
    int32_t sx, sy;

    (void)raw_z2;

    if (p == NULL) {
        return false;
    }

    if (!TSC2007_ReadRaw(&raw_x, &raw_y, &raw_z1, &raw_z2)) {
        return false;
    }

    /*
     * Preserve the same orientation transform your old driver used:
     * screen X comes from raw Y,
     * screen Y comes from raw X and is flipped vertically.
     */
    sx = clamp_map_i32((int32_t)raw_y,
                       (int32_t)TOUCH_RAW_Y_MIN,
                       (int32_t)TOUCH_RAW_Y_MAX,
                       0,
                       (int32_t)TOUCH_SCREEN_W - 1);

    sy = clamp_map_i32((int32_t)raw_x,
                       (int32_t)TOUCH_RAW_X_MIN,
                       (int32_t)TOUCH_RAW_X_MAX,
                       0,
                       (int32_t)TOUCH_SCREEN_H - 1);

    sy = ((int32_t)TOUCH_SCREEN_H - 1) - sy;

    p->x = (uint16_t)sx;
    p->y = (uint16_t)sy;
    p->z = raw_z1;

    return true;
}
