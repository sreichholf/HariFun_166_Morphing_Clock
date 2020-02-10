#include <Preferences.h>
#include <Prefs.h>

rgb24 colorBorder = COLOR_WHITE;
rgb24 colorColon = COLOR_LIGHT_BLUE;
rgb24 colorDate = COLOR_WHITE;
rgb24 colorDigit = COLOR_WHITE;
rgb24 colorHighlight = COLOR_LIGHT_BLUE;

Preferences preferences;

void setupPreferences()
{
	preferences.begin("MorphingClock", false);
	loadPreferences();
}

uint32_t colorFromRgb(const rgb24 &rgb)
{
	return rgb.red << 16 | rgb.green << 8 | rgb.blue;
}

rgb24 rgbFromColor(uint32_t color)
{
	return rgb24(color >> 16 & 0xff, color >> 8 & 0xff, color & 0xff);
}

rgb24 loadColor(const char* key, const rgb24 &defaultColor)
{
	uint32_t defaultValue = colorFromRgb(defaultColor);
	uint32_t col = preferences.getUInt(key, defaultValue);
	return rgbFromColor(col);
}

void saveColor(const char* key, const rgb24& color)
{
	preferences.putUInt(key, colorFromRgb(color));
}

void loadPreferences()
{
	colorBorder = loadColor("colorBorder", colorBorder);
	colorColon = loadColor("colorColon", colorColon);
	colorDate = loadColor("colorDate", colorDate);
	colorDigit = loadColor("colorDigit", colorDigit);
	colorHighlight = loadColor("colorHighlight", colorHighlight);
}

void savePreferences()
{
	saveColor("colorBorder", colorBorder);
	saveColor("colorColon", colorBorder);
	saveColor("colorDate", colorBorder);
	saveColor("colorDigit", colorBorder);
	saveColor("colorHighlight", colorBorder);
}

void resetPreferences()
{
	colorBorder = COLOR_WHITE;
	colorColon = COLOR_LIGHT_BLUE;
	colorDate = COLOR_WHITE;
	colorDigit = COLOR_WHITE;
	colorHighlight = COLOR_LIGHT_BLUE;
	savePreferences();
}