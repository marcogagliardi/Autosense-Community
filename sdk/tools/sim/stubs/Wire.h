#pragma once
#include <Arduino.h>
class TwoWire { public:
 bool begin(int sda = -1, int scl = -1) { return true; }
 void beginTransmission(uint8_t) {}
 uint8_t endTransmission() { return 0; }
 void setClock(uint32_t) {}
};
extern TwoWire Wire;
