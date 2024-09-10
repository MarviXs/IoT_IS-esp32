#ifndef SHT41_H
#define SHT41_H

#include <Arduino.h>
#include <SensirionI2cSht4x.h>

static float sht_41_temperature = 0.0;
static float sht_41_humidity = 0.0;

void initSHT41();
void readSHT41(void *parameter);

#endif
