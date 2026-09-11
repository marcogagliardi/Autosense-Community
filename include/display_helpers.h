#pragma once
#include <Arduino.h>
#include <vector>

// OLED display helpers.
// Include this header when your plugin needs to display readings on the OLED screen.
// Coordinate guidelines:
// - 64px OLED (3 lines): Line 1: Y=16, Line 2: Y=32, Line 3: Y=50
// - 32px OLED (2 lines): Line 1: Y=14, Line 2: Y=24

void DisplayText(const String text, int x, int y, int textSize = 1);
void DisplayClear();
void DisplayClearLine(int y, int lines = 1, int lineLength = 128);
void DisplayMultipleLines(const std::vector<String>& lines);
void DisplayVersion();
int GetOledHeight();
