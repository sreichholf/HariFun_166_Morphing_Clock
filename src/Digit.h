#ifndef DIGIT_H
#define DIGIT_H

#include <Arduino.h>

#include <SmartMatrix3.h> // https://github.com/2dom/PxMatrix
#include <FastLED.h>

class Digit {
  
  public:
    Digit(SMLayerBackground<rgb24, 0U>* d, byte value, uint16_t xo, uint16_t yo, const rgb24& color);
    void Draw(byte value);
    void Morph(void);
    void SetValue(byte value);
    void SetColor(const rgb24 &color);
    // void DrawColon();
    void DrawColon(const rgb24 &c);

  private:
    SMLayerBackground<rgb24, 0U>* _display;
    byte _oldvalue;
    byte _value;
    byte _morphcnt;
    rgb24 _color;
    uint16_t xOffset;
    uint16_t yOffset;

    void drawPixel(uint16_t x, uint16_t y, const rgb24 &c);
    void drawFillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const rgb24 &c);
    void drawLine(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2, const rgb24 &c);
    void drawSeg(byte seg);
    void Morph2();
    void Morph3();
    void Morph4();
    void Morph5();
    void Morph6();
    void Morph7();
    void Morph8();
    void Morph9();
    void Morph0();
    void Morph1();
};

#endif
