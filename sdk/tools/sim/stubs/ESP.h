#pragma once
#include <Arduino.h>
class EspClass { public:
 uint32_t getSketchSize() { return 0; } uint32_t getFreeSketchSpace() { return 0; } const char *getSdkVersion() { return ""; } uint32_t getFreeHeap() { return 0; }
 uint32_t getFlashChipSize() { return 0; } void restart() {} uint32_t getCpuFreqMHz() { return 0; } int getChipRevision() { return 0; } const char *getChipModel() { return ""; }
 int getChipCores() { return 1; } uint64_t getEfuseMac() { return 0; } uint32_t getChipId() { return 0; } uint32_t getFlashChipId() { return 0; }
};
extern EspClass ESP;
