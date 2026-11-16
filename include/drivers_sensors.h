#ifndef DRIVERS_SENSORS_H
#define DRIVERS_SENSORS_H

#include <stdbool.h>
#include <stdint.h>

bool DHT22_Read(float *temperature, float *humidity);
int LDR_ReadPercent(void);
bool PIR_ReadMotion(void);

bool Sensors_Init(void);
void Encoder_Init(void);
int Encoder_GetDelta(void);
bool Encoder_ReadButton(void);
bool Buzzer_Set(bool on);
void Delay_us(uint32_t us);
void Serial_Print(const char *text);

#endif
