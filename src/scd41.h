#ifndef SCD41_H
#define SCD41_H

#include <Arduino.h>
#include <SensirionI2CScd4x.h>

void initSCD41();
void readSCD41(void *parameter);

#endif
