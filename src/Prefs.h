#pragma once

#include <MatrixCommon.h>

const rgb24 COLOR_WHITE(255,255,255);
const rgb24 COLOR_BLACK(0,0,0);
const rgb24 COLOR_RED(255,0,0);
const rgb24 COLOR_LIGHT_BLUE(0x44,0xEE,0xFF);

extern rgb24 colorBorder;
extern rgb24 colorColon;
extern rgb24 colorDate;
extern rgb24 colorDigit;
extern rgb24 colorHighlight;

void setupPreferences();
void loadPreferences();
void saveColor(const char* key, const rgb24& color);
void savePreferences();
void resetPreferences();