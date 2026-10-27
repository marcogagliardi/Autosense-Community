#pragma once
#include <Arduino.h>
#define SPI_MODE0 0
#define SPI_MSBFIRST 1
#define SPI_LSBFIRST 0
#define MSBFIRST 1
#define LSBFIRST 0
typedef int BitOrder;
class SPISettings { public: SPISettings(uint32_t = 0, uint8_t = 0, uint8_t = 0) {} };
class SPIClass { public: void begin() {} void beginTransaction(SPISettings) {} void endTransaction() {} uint8_t transfer(uint8_t) { return 0; } };
extern SPIClass SPI;
