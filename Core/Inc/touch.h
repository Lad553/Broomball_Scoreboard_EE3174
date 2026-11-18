#ifndef TOUCH_H
#define TOUCH_H

// Includes
#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Structs
typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t z; // touch pressure
} TouchPoint;

// Defines: calibration values; adjust if touches appear offset from screen edges
#ifndef TOUCH_RAW_X_MIN
#define TOUCH_RAW_X_MIN  200U
#endif

#ifndef TOUCH_RAW_X_MAX
#define TOUCH_RAW_X_MAX  3900U
#endif

#ifndef TOUCH_RAW_Y_MIN
#define TOUCH_RAW_Y_MIN  200U
#endif

#ifndef TOUCH_RAW_Y_MAX
#define TOUCH_RAW_Y_MAX  3900U
#endif

#ifndef TOUCH_SCREEN_W
#define TOUCH_SCREEN_W   320U
#endif

#ifndef TOUCH_SCREEN_H
#define TOUCH_SCREEN_H   240U
#endif

#ifndef TOUCH_Z_THRESHOLD
#define TOUCH_Z_THRESHOLD 100U
#endif

// Function prototypes
void TSC2007_Init(I2C_HandleTypeDef *hi2c);
bool TSC2007_ReadRaw(uint16_t *x, uint16_t *y, uint16_t *z1, uint16_t *z2);
bool Touch_ReadScreenPoint(TouchPoint *p);

#ifdef __cplusplus
}
#endif

#endif
