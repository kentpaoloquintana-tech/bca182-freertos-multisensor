#ifndef DRIVERS_OLED_H
#define DRIVERS_OLED_H

#include <stdint.h>

void OLED_Init(void);
void OLED_SetDisplay(bool enabled);
void OLED_Clear(void);
void OLED_DrawString(uint8_t x, uint8_t page, const char *text);
void OLED_Update(void);

void Format_Fixed1(char *out, const char *prefix, float value, const char *suffix);
void Format_Int(char *out, const char *prefix, int value, const char *suffix);

#endif
