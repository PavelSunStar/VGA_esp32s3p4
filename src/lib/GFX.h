#pragma once

#include "..\VGA_esp32s3p4.h"

// ---- 8 bit (RGB332) ----
inline int R8(uint8_t c) { return (c >> 5) & 0x07; }
inline int G8(uint8_t c) { return (c >> 2) & 0x07; }
inline int B8(uint8_t c) { return  c       & 0x03; }

// ---- 16 bit (RGB565) ----
inline int R16(uint16_t c) { return (c >> 11) & 0x1F; }
inline int G16(uint16_t c) { return (c >> 5)  & 0x3F; }
inline int B16(uint16_t c) { return  c        & 0x1F; }

class GFX{  
    public:
        GFX(VGA_esp32s3p4& vga);    
        ~GFX();    

        uint16_t getCol(uint8_t r, uint8_t g, uint8_t b);
        uint16_t getBlendCol(uint16_t dst, uint16_t src, uint8_t a);
        uint16_t getPixel(int x, int y);

        void cls(uint16_t col);
        void putPixel(int x, int y, uint16_t col);
        void putBlendPixel(int x, int y, uint16_t src, uint8_t a);
        void putPixelRGB(int x, int y, uint8_t r, uint8_t g, uint8_t b);
        void hLine(int x1, int y, int x2, uint16_t col);
        void orHLine(int x1, int y, int x2, uint16_t col);
        void xorHLine(int x1, int y, int x2, uint16_t col);
        void vLine(int x, int y1, int y2, uint16_t col);
        void rect(int x1, int y1, int x2, int y2, uint16_t col);
        void fillRect(int x1, int y1, int x2, int y2, uint16_t col);
        void roundRect(int x1, int y1, int x2, int y2, int r, uint16_t col);
        void fillRoundRect(int x1, int y1, int x2, int y2, int r, uint16_t col);
        void line(int x1, int y1, int x2, int y2, uint16_t col);
        void lineAngle(int x, int y, int len, int angle, uint16_t col);
        void triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint16_t col);
        void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint16_t col);
        void circle(int xc, int yc, int r, uint16_t col);
        void fillCircle(int xc, int yc, int r, uint16_t col);
        void circleHelper(int xc, int yc, int r, uint8_t corner, uint16_t col);
        void fillCircleHelper(int xc, int yc, int r, uint8_t corner, int ybase, uint16_t col);
        void polygon(int x, int y, int radius, int sides, int ang, uint16_t col);
        void fillPolygon(int x, int y, int radius, int sides, int rotation, uint16_t col);
        void star(int x, int y, int radius, int sides, int rotation, uint16_t col);
        void fillStar(int x, int y, int radius, int sides, int rotation, uint16_t col);
        void star2(int x, int y, int radius1, int radius2, int sides, int rotation, uint16_t col);
        void fillStar2(int x, int y, int radius1, int radius2, int sides, int rotation, uint16_t col);         
        void ellipse(int xc, int yc, int rx, int ry, uint16_t col); 
        void fillEllipse(int xc, int yc, int rx, int ry, uint16_t col); 
        void arc(int xc, int yc, int r, int angle1, int angle2, uint16_t col); 

        void putChar(int x, int y, char ch, uint16_t col);
        void putString(int x, int y, TextAlign align, const char* s, uint16_t col);
        void putText(int cx, int cy, TextAlign align, const char* s, uint16_t col);
        void putText(int cx, int cy, TextAlign align, int value, uint16_t col);
        void putText(int cx, int cy, TextAlign align, float value, uint16_t col);
        void putText(int cx, int cy, TextAlign align, const String& s, uint16_t col);

        //Effect
        void blur();
                
    protected:        
        VGA_esp32s3p4& _vga;

        int stringLength(const char* s);
        uint8_t utf8_to_font(const uint8_t*& p);
        const uint8_t* font = nullptr; 
        uint8_t fontWidth;
        uint8_t fontHeight;
};    

