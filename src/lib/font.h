#pragma once

#include <Arduino.h>
#include "..\VGA_esp32s3p4.h"

class Font{
    public:
        Font(VGA_esp32s3p4& vga);
        ~Font();

        int Width(){return _width;};
        int Height(){return _height;};

        int stringLength(const char* s);
        uint8_t utf8_to_font(const uint8_t*& p);   

        void putChar(int x, int y, char ch, uint16_t col);

        void putString(int x, int y, const char* s, uint16_t col = 0xFFFF, TextAlign align = LEFT);
        void putText(int x, int y, const char* s, uint16_t col = 0xFFFF, TextAlign align = LEFT);
        void putText(int x, int y, uint32_t value, uint16_t col = 0xFFFF, TextAlign align = LEFT);
        void putText(int x, int y, int value, uint16_t col = 0xFFFF, TextAlign align = LEFT);
        void putText(int x, int y, float value, uint16_t col = 0xFFFF, TextAlign align = LEFT);
        void putText(int x, int y, const String& s, uint16_t col = 0xFFFF, TextAlign align = LEFT);

    protected:
        uint8_t* _font = nullptr; 
        uint8_t* _tmpBuf = nullptr;
        uint8_t _width;
        uint8_t _height;
        VGA_esp32s3p4& _vga;  
};

/*
    auto line = [&](int lx){

    };
        
void Font::putChar(int x, int y, char ch, uint16_t col, uint8_t scale){
    scale = std::clamp((int)scale, 1, 255);

    if (x > _vga.vX2() || y > _vga.vY2()) return;
    int xx = x + _width - 1;
    int yy = y + _height - 1;
    if (xx < _vga.vX1() || yy < _vga.vY1()) return;

    int skip = _vga.Width() - _width;
    uint8_t* c = _font + (ch * 8);
    if (_vga.BPP() == 16){
        uint16_t* scr = _vga.lineBuf16[y + _vga.backBufLine] + x;

        while (y++ <= yy){
            uint8_t line = *c++;
            for (int px = 0; px < _width; px++){
                if (line & (0x80 >> px)) *scr = col;
                scr++;
            }
            scr += skip;
        }
    } else {
        uint8_t color = (uint8_t)(col & 0xFF);
        uint8_t* scr = _vga.lineBuf8[y + _vga.backBufLine] + x;

        if (scale == 1){
            while (y++ <= yy){
                uint8_t line = *c++;
                for (int px = 0; px < _width; px++){
                    if (line & (0x80 >> px)) *scr = color;
                    scr++;
                }
                scr += skip;
            }
        } else {
            skip -= _width * (scale - 1);
            while (y++ <= yy){
                uint8_t line = *c++;
                for (int px = 0; px < _width; px++){
                    for (int z = 0; z < scale; z++){
                        if (line & (0x80 >> px)) *scr = color;
                        scr++;
                    }
                }
                scr += skip;
            }            
        }
    }
}
*/