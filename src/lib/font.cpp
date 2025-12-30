#include "font.h"
#include "../Fonts/default.h"

Font::Font(VGA_esp32s3p4& vga) : _vga(vga){
    _font = (uint8_t*)FONT_8x8_Def;
    _width = *_font++;
    _height = *_font++;

    _tmpBuf = (uint8_t*)malloc(2048);
    memset(_tmpBuf, 0, 2048);
}

Font::~Font(){
    free(_tmpBuf);
}

uint8_t Font::utf8_to_font(const uint8_t*& p)
{
    uint8_t c1 = *p++;

    // ASCII
    if (c1 < 128) return c1;

    uint8_t c2 = *p++;

    // Кириллица
    if (c1 == 0xD0) {
        if (c2 >= 0x90 && c2 <= 0xBF)  // А..п
            return 0x80 + (c2 - 0x90); // 128..175

        if (c2 == 0x81)  // Ё
            return 240;  // CP866 Ё = 240
    }

    if (c1 == 0xD1) {
        if (c2 >= 0x80 && c2 <= 0x8F) // р..я
            return 0xE0 + (c2 - 0x80); // 224..239

        if (c2 == 0x91) // ё
            return 241; // CP866 ё = 241
    }

    return '?';
}

int Font::stringLength(const char* s){
    int len = 0;
    const uint8_t* p = (const uint8_t*)s;

    while (*p)
    {
        utf8_to_font(p); 
        len++;
    }
    return len;
}

void Font::putChar(int x, int y, char ch, uint16_t col){
    //scale = std::clamp((int)scale, 1, 255);

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

        while (y++ <= yy){
                uint8_t line = *c++;
                for (int px = 0; px < _width; px++){
                    if (line & (0x80 >> px)) *scr = color;
                    scr++;
                }
                scr += skip;
        }
    }
}

/*
void Font::putChar(int x, int y, char ch, uint16_t col, uint8_t scale)
{
    scale = std::clamp((int)scale, 1, 255);

    const int width  = _width;
    const int height = _height;

    const int scaled_w = width * scale;
    const int scaled_h = height * scale;

    int x2 = x + scaled_w - 1;
    int y2 = y + scaled_h - 1;

    // Полное отсечение
    if (x > _vga.vX2() || y > _vga.vY2() || x2 < _vga.vX1() || y2 < _vga.vY1())
        return;

    uint8_t* glyph = _font + (uint16_t(ch) * height);

    int fbw = _vga.Width();

    // -------------------------------------------------------------------
    // 16 BPP
    // -------------------------------------------------------------------
    if (_vga.BPP() == 16)
    {
        uint16_t* scr = _vga.lineBuf16[y + _vga.backBufLine] + x;
        uint16_t color = col;

        for (int row = 0; row < height; row++)
        {
            uint8_t bits = glyph[row];

            // вертикальный масштаб
            for (int v = 0; v < scale; v++)
            {
                int cur_y = y + row * scale + v;
                if (cur_y > _vga.vY2()) return;

                uint16_t* p = scr;

                // горизонтальный масштаб
                for (int px = 0; px < width; px++)
                {
                    uint16_t pix = (bits & (0x80 >> px)) ? color : 0;

                    for (int h = 0; h < scale; h++)
                        *p++ = pix;
                }

                scr += fbw; // переход на следующую строку framebuffer
            }
        }

        return;
    }

    // -------------------------------------------------------------------
    // 8 BPP
    // -------------------------------------------------------------------
    uint8_t color8 = col & 0xFF;

    if (scale == 1)
    {
        // Быстрый путь
        uint8_t* scr = _vga.lineBuf8[y + _vga.backBufLine] + x;
        int skip = fbw - width;

        for (int row = 0; row < height; row++)
        {
            uint8_t bits = glyph[row];

            int cur_y = y + row;
            if (cur_y > _vga.vY2()) return;

            for (int px = 0; px < width; px++)
            {
                if (bits & (0x80 >> px)) *scr = color8;
                scr++;
            }

            scr += skip;
        }

        return;
    }

    // Масштабированный путь
    uint8_t* scr = _vga.lineBuf8[y + _vga.backBufLine] + x;

    for (int row = 0; row < height; row++)
    {
        uint8_t bits = glyph[row];

        for (int v = 0; v < scale; v++)
        {
            int cur_y = y + row * scale + v;
            if (cur_y > _vga.vY2()) return;

            uint8_t* p = scr;

            for (int px = 0; px < width; px++)
            {
                uint8_t pix = (bits & (0x80 >> px)) ? color8 : 0;

                for (int h = 0; h < scale; h++)
                    *p++ = pix;
            }

            scr += fbw;
        }
    }
}

*/
void Font::putString(int x, int y, const char* s, uint16_t col, TextAlign align){
    int len = stringLength(s);
    int w = len * _width;

    switch (align)
    {
        case CENTER:
            x -= w / 2;
            y -= _height / 2;
            break;

        case RIGHT:
            x -= w;
            break;

        case LEFT:
        default:
            break;
    }

    const uint8_t* p = (const uint8_t*)s;

    while (*p)
    {
        uint8_t ch = utf8_to_font(p);
        putChar(x, y, ch, col);
        x += _width;
    }
}

void Font::putText(int x, int y, const char* s, uint16_t col, TextAlign align) {
    putString(x, y, s, col, align);
}

void Font::putText(int x, int y, uint32_t value, uint16_t col, TextAlign align)
{
    char buf[16];
    ultoa(value, buf, 10);
    putString(x, y, buf, col, align);
}

void Font::putText(int x, int y, int value, uint16_t col, TextAlign align) {
    char buf[16];
    itoa(value, buf, 10);
    putString(x, y, buf, col, align);
}

void Font::putText(int x, int y, float value, uint16_t col, TextAlign align) {
    char buf[32];
    dtostrf(value, 0, 2, buf);
    putString(x, y, buf, col, align);
}

void Font::putText(int x, int y, const String& s, uint16_t col, TextAlign align) {
    putString(x, y, s.c_str(), col, align);
}