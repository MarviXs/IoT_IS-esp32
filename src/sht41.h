#ifndef SHT41_H
#define SHT41_H

#include <Arduino.h>
#include <SensirionI2cSht4x.h>

void initSHT41();
void readSHT41(void *parameter);

#endif
