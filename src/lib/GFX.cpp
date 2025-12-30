#include <Arduino.h>
#include "GFX.h"
#include "../Fonts/default.h"

GFX::GFX(VGA_esp32s3p4& vga) : _vga(vga){
    font = FONT_8x8_Def;
    fontWidth = *font++;
    fontHeight = *font++;

    for (int i = 0; i < 256; ++i) {
        alpha_mul[i]  = i;
        alpha_imul[i] = 255 - i;
    }    
}

GFX::~GFX(){

}

uint16_t GFX::getCol(uint8_t r, uint8_t g, uint8_t b){
    if (_vga.BPP() == 16) {
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);      
    } 
    else if (_vga.BPP() == 8) {
        return ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
    }
    
    return 0;
}

uint16_t GFX::getBlendCol(uint16_t dst, uint16_t src, uint8_t a){
if (a == 0)   return dst;
    if (a == 255) return src;

    uint32_t ia = 255 - a;

    if (_vga.BPP() == 16) {
        uint32_t dr = (dst >> 11) & 31, dg = (dst >> 5) & 63, db = dst & 31;
        uint32_t sr = (src >> 11) & 31, sg = (src >> 5) & 63, sb = src & 31;

        // Быстрое деление на 255 с округлением
        auto blend = [&](uint32_t s, uint32_t d) -> uint32_t {
            uint32_t x = s * a + d * ia;
            return (x + (x >> 8) + 0x80) >> 8;  // ~ /255 с округлением
        };

        uint32_t r = blend(sr, dr);
        uint32_t g = blend(sg, dg);
        uint32_t b = blend(sb, db);

        return (r << 11) | (g << 5) | b;
    } else {
        // RGB332
        uint8_t d8 = dst, s8 = src;
        uint32_t dr = (d8 >> 5) & 7, dg = (d8 >> 2) & 7, db = d8 & 3;
        uint32_t sr = (s8 >> 5) & 7, sg = (s8 >> 2) & 7, sb = s8 & 3;

        auto blend = [&](uint32_t s, uint32_t d) -> uint32_t {
            uint32_t x = s * a + d * ia;
            return (x + (x >> 8) + 0x80) >> 8;
        };

        return (blend(sr, dr) << 5) | (blend(sg, dg) << 2) | blend(sb, db);
    }
}
/*
uint16_t GFX::getBlendCol(uint16_t dst, uint16_t src, uint8_t a){
    // a: 0..255
    if (a == 0)   return dst;
    if (a == 255) return src;

    if (_vga.BPP() == 16){
        uint32_t dr = (dst >> 11) & 31, dg = (dst >> 5) & 63, db = dst & 31;
        uint32_t sr = (src >> 11) & 31, sg = (src >> 5) & 63, sb = src & 31;

        uint32_t ia = 255 - a;

        uint32_t r = (sr * a + dr * ia + 127) / 255;
        uint32_t g = (sg * a + dg * ia + 127) / 255;
        uint32_t b = (sb * a + db * ia + 127) / 255;

        return (uint16_t)((r << 11) | (g << 5) | b);
    } else {
        // RGB332 в одном байте
        uint8_t dst8 = (uint8_t)dst;
        uint8_t src8 = (uint8_t)src;

        uint32_t dr = (dst8 >> 5) & 7, dg = (dst8 >> 2) & 7, db = dst8 & 3;
        uint32_t sr = (src8 >> 5) & 7, sg = (src8 >> 2) & 7, sb = src8 & 3;

        uint32_t ia = 255 - a;

        uint32_t r = (sr * a + dr * ia + 127) / 255; // 0..7
        uint32_t g = (sg * a + dg * ia + 127) / 255; // 0..7
        uint32_t b = (sb * a + db * ia + 127) / 255; // 0..3

        return (uint16_t)((r << 5) | (g << 2) | b);
    }
}
*/

void GFX::cls(uint16_t col){
    if (_vga.BPP() == 16) {
        uint16_t* scr = PTR_OFFSET_T(_vga.buf, _vga.backBuf, uint16_t);
        uint16_t* cpy = scr;

        int size = 0;
        while (size++ <= _vga.XX()) *scr++ = col;   

        int dummy = 1; 
        int lines = _vga.YY();  
        int copyBytes = _vga.LineSize();
        int offset = _vga.Width();
     
        while (lines > 0){ 
            if (lines >= dummy){
                memcpy(scr, cpy, copyBytes);
                lines -= dummy;
                scr += offset;
                copyBytes <<= 1;
                offset <<= 1;
                dummy <<= 1;
            } else {
                copyBytes = _vga.LineSize() * lines;
                memcpy(scr, cpy, copyBytes);
                break;
            }
        }            
    } else {
        uint8_t* scr = PTR_OFFSET_T(_vga.buf, _vga.backBuf, uint8_t);
        memset(scr, (uint8_t)col, _vga.FrameFullSize());
    }
}

uint16_t GFX::getPixel(int x, int y){
    if (x < _vga.vX1() || y < _vga.vY1() || x > _vga.vX2() ||  y > _vga.vY2()) return 0;

    if (_vga.BPP() == 16){
        return _vga.lineBuf16[y + _vga.backBufLine][x];
    } else {
        return _vga.lineBuf8[y + _vga.backBufLine][x];
    }

    return 0;
}

void GFX::putPixel(int x, int y, uint16_t col){
    if (x < _vga.vX1() || y < _vga.vY1() || x > _vga.vX2() ||  y > _vga.vY2()) return;

    if (_vga.BPP() == 16){
        _vga.lineBuf16[y + _vga.backBufLine][x] = col;
    } else {
        _vga.lineBuf8[y + _vga.backBufLine][x] = (uint8_t)(col & 0xFF);
    }
}

void GFX::putBlendPixel(int x, int y, uint16_t src, uint8_t a) {
    if (x < _vga.vX1() || y < _vga.vY1() || x > _vga.vX2() || y > _vga.vY2()) return;
    if (a == 0) return;

    // Краевой случай: полная непрозрачность — просто пишем без блендинга
    if (a == 255) {
        putPixel(x, y, src);  // предполагается, что есть быстрый putPixel без проверки границ
        return;
    }

    // Быстрое "деление на 255 с округлением": (x + (x>>8) + 0x80) >> 8
    // Эквивалентно (x + 127) / 255 с погрешностью < 0.5
    auto fast_div255 = [](uint32_t val) -> uint32_t {
        return (val + (val >> 8) + 0x80) >> 8;
    };

    if (_vga.BPP() == 16) {
        uint16_t* p = _vga.lineBuf16[y + _vga.backBufLine] + x;
        uint16_t dst = *p;

        uint32_t dr = (dst >> 11) & 31;
        uint32_t dg = (dst >> 5)  & 63;
        uint32_t db = dst & 31;

        uint32_t sr = (src >> 11) & 31;
        uint32_t sg = (src >> 5)  & 63;
        uint32_t sb = src & 31;

        // Используем таблицы вместо вычисления a и (255-a) каждый раз
        uint32_t ma  = alpha_mul[a];
        uint32_t mia = alpha_imul[a];

        uint32_t r = fast_div255(sr * ma + dr * mia);
        uint32_t g = fast_div255(sg * ma + dg * mia);
        uint32_t b = fast_div255(sb * ma + db * mia);

        *p = (uint16_t)((r << 11) | (g << 5) | b);

    } else { // RGB332
        uint8_t* p = _vga.lineBuf8[y + _vga.backBufLine] + x;
        uint8_t dst8 = *p;
        uint8_t src8 = (uint8_t)src;

        uint32_t dr = (dst8 >> 5) & 7;
        uint32_t dg = (dst8 >> 2) & 7;
        uint32_t db = dst8 & 3;

        uint32_t sr = (src8 >> 5) & 7;
        uint32_t sg = (src8 >> 2) & 7;
        uint32_t sb = src8 & 3;

        uint32_t ma  = alpha_mul[a];
        uint32_t mia = alpha_imul[a];

        uint32_t r = fast_div255(sr * ma + dr * mia);
        uint32_t g = fast_div255(sg * ma + dg * mia);
        uint32_t b = fast_div255(sb * ma + db * mia);

        *p = (uint8_t)((r << 5) | (g << 2) | b);
    }
}

void GFX::putPixelRGB(int x, int y, uint8_t r, uint8_t g, uint8_t b){
    if (x < _vga.vX1() || y < _vga.vY1() || x > _vga.vX2() ||  y > _vga.vY2()) return;

    if (_vga.BPP() == 16){
        _vga.lineBuf16[y + _vga.backBufLine][x] = (uint16_t)((r >> 3) << 11) | ((g >> 2) << 5) | ((b >> 3));
    } else {
        _vga.lineBuf8[y + _vga.backBufLine][x] = ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
    } 
}

void GFX::hLine(int x1, int y, int x2, uint16_t col){
    if (y < _vga.vY1() || y > _vga.vY2()) return;

    if (x1 > x2) std::swap(x1, x2);
    if (x1 > _vga.vX2() || x2 < _vga.vX1()) return;
    x1 = std::max(_vga.vX1(), x1);
    x2 = std::min(_vga.vX2(), x2);

    if (_vga.BPP() == 16){ 
        uint16_t* scr = _vga.lineBuf16[y + _vga.backBufLine] + x1;
        while (x1++ <= x2) *scr++ = col;
    } else {
        uint8_t* scr = _vga.lineBuf8[y + _vga.backBufLine] + x1;
        memset(scr, (uint8_t)(col & 0xFF), x2 - x1 + 1);
    }        
}

void GFX::orHLine(int x1, int y, int x2, uint16_t col){
    if (y < _vga.vY1() || y > _vga.vY2()) return;

    if (x1 > x2) std::swap(x1, x2);
    if (x1 > _vga.vX2() || x2 < _vga.vX1()) return;
    x1 = std::max(_vga.vX1(), x1);
    x2 = std::min(_vga.vX2(), x2);

    if (_vga.BPP() == 16){
        uint16_t* scr = _vga.lineBuf16[y + _vga.backBufLine] + x1;
        while (x1++ <= x2) *scr++ |= col;
    } else {
        uint8_t* scr = _vga.lineBuf8[y + _vga.backBufLine] + x1;
        uint8_t c = (uint8_t)(col & 0xFF);
        while (x1++ <= x2) *scr++ |= c;
    }
}

void GFX::xorHLine(int x1, int y, int x2, uint16_t col){
    if (y < _vga.vY1() || y > _vga.vY2()) return;

    if (x1 > x2) std::swap(x1, x2);
    if (x1 > _vga.vX2() || x2 < _vga.vX1()) return;
    x1 = std::max(_vga.vX1(), x1);
    x2 = std::min(_vga.vX2(), x2);

    if (_vga.BPP() == 16){
        uint16_t* scr = _vga.lineBuf16[y + _vga.backBufLine] + x1;
        while (x1++ <= x2) *scr++ ^= col;
    } else {
        uint8_t* scr = _vga.lineBuf8[y + _vga.backBufLine] + x1;
        uint8_t c = (uint8_t)(col & 0xFF);
        while (x1++ <= x2) *scr++ ^= c;
    }
}

void GFX::vLine(int x, int y1, int y2, uint16_t col){
    if (x < _vga.vX1() || x > _vga.vX2()) return;

    if (y1 > y2) std::swap(y1, y2);
    if (y1 > _vga.vY2() || y2 < _vga.vY1()) return;
    y1 = std::max(_vga.vY1(), y1);
    y2 = std::min(_vga.vY2(), y2);

    if (_vga.BPP() == 16){ 
        uint16_t* scr = _vga.lineBuf16[y1 + _vga.backBufLine] + x;
        while (y1++ <= y2){ 
            *scr = col;
            scr += _vga.Width();
        }
    } else {
        uint8_t* scr = _vga.lineBuf8[y1 + _vga.backBufLine] + x;
        uint8_t color = (uint8_t)(col & 0xFF);
        while (y1++ <= y2){ 
            *scr = color;
            scr += _vga.Width();
        }       
    }
}

void GFX::rect(int x1, int y1, int x2, int y2, uint16_t col){
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    if (x1 > _vga.vX2() || y1 > _vga.vY2() || x2 < _vga.vX1() || y2 < _vga.vY1()){
        return;
    } else if (x1 >= _vga.vX1() && y1 >= _vga.vY1() && x2 <= _vga.vX2() && y2 <= _vga.vY2()){
        int sizeX = x2 - x1 + 1; 
        int sizeX2X = sizeX << 1;
        int sizeY = y2 - y1 - 1;
        int width = _vga.Width();
        int skip1 = x2 - x1; 
        int skip2 = width - x2 + x1;
        int offset = width * y1 + x1;

        if (_vga.BPP() == 16){
            uint16_t* scr = _vga.lineBuf16[y1 + _vga.backBufLine] + x1;
            uint16_t* cpy = scr;
            
            while (sizeX-- > 0) *scr++ = col;
            scr += skip2 - 1;

            while (sizeY-- > 0){
                *scr = col; scr += skip1;
                *scr = col; scr += skip2;
            }
            memcpy(scr, cpy, sizeX2X);
        } else {
            uint8_t* scr = _vga.lineBuf8[y1 + _vga.backBufLine] + x1;
            uint8_t color = (uint8_t) col;

            memset(scr, color, sizeX);
            scr += width;

            while (sizeY-- > 0){
                *scr = color; scr += skip1;
                *scr = color; scr += skip2;
            }
            memset(scr, color, sizeX);
        }
    } else {
        hLine(x1, y1, x2, col); 
        hLine(x1, y2, x2, col); 
        vLine(x1, y1, y2, col);  
        vLine(x2, y1, y2, col);  
    }
}

void GFX::fillRect(int x1, int y1, int x2, int y2, uint16_t col){
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    if (x1 > _vga.vX2() || y1 > _vga.vY2() || x2 < _vga.vX1() || y2 < _vga.vY1()) return; 

    x1 = std::max(_vga.vX1(), x1);
    x2 = std::min(_vga.vX2(), x2);
    y1 = std::max(_vga.vY1(), y1);
    y2 = std::min(_vga.vY2(), y2);
    
    int sizeX = x2 - x1 + 1;
    int sizeY = y2 - y1 + 1;     
    int width = _vga.Width();

    if (_vga.BPP() == 16){
        uint16_t* scr = _vga.lineBuf16[y1 + _vga.backBufLine] + x1;
        uint16_t* savePos = scr;

        int skip = width - x2 + x1 - 1;
        int copyBytes = sizeX << 1;
        int saveSizeX = sizeX; 

        while (sizeX-- > 0) *scr++ = col;
        scr += skip;
        sizeY--;

        while (sizeY-- > 0){
            memcpy(scr, savePos, copyBytes);
            scr += width;
        }
    } else {
        uint8_t* scr = _vga.lineBuf8[y1 + _vga.backBufLine] + x1;
        uint8_t color = (uint8_t)(col & 0xFF);
        
        while (sizeY-- > 0){
            memset(scr, color, sizeX);
            scr += width;
        }    
    }     
}

void GFX::roundRect(int x1, int y1, int x2, int y2, int r, uint16_t col){
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    // Боковые линии
    vLine(x1, y1 + r, y2 - r, col);
    vLine(x2, y1 + r, y2 - r, col);
    hLine(x1 + r, y1, x2 - r, col);
    hLine(x1 + r, y2, x2 - r, col);

    // Углы
    int xc, yc;
    xc = x1 + r; yc = y1 + r;
    circleHelper(xc, yc, r, 1, col);  // верхний левый
    xc = x2 - r; yc = y1 + r;
    circleHelper(xc, yc, r, 2, col);  // верхний правый
    xc = x2 - r; yc = y2 - r;
    circleHelper(xc, yc, r, 4, col);  // нижний правый
    xc = x1 + r; yc = y2 - r;
    circleHelper(xc, yc, r, 8, col);  // нижний левый
}

void GFX::fillRoundRect(int x1, int y1, int x2, int y2, int r, uint16_t col){
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    // Центральная часть
    fillRect(x1 + r, y1, x2 - r, y2, col);

    // Левые и правые полоски
    fillRect(x1, y1 + r, x1 + r, y2 - r, col);
    fillRect(x2 - r, y1 + r, x2, y2 - r, col);

    // Углы
    int xc, yc;
    xc = x1 + r; yc = y1 + r;
    fillCircleHelper(xc, yc, r, 1, y1, col); // верхний левый
    xc = x2 - r; yc = y1 + r;
    fillCircleHelper(xc, yc, r, 2, y1, col); // верхний правый
    xc = x2 - r; yc = y2 - r;
    fillCircleHelper(xc, yc, r, 4, y2, col); // нижний правый
    xc = x1 + r; yc = y2 - r;
    fillCircleHelper(xc, yc, r, 8, y2, col); // нижний левый
}

void GFX::line(int x1, int y1, int x2, int y2, uint16_t col){
    if (x1 == x2) {
        vLine(x1, y1, y2, col);
    } else if (y1 == y2){
        hLine(x1, y1, x2, col);
    } else {
        // Алгоритм Брезенхэма
        int dx = abs(x2 - x1);
        int dy = abs(y2 - y1);
        int sx = (x1 < x2) ? 1 : -1;
        int sy = (y1 < y2) ? 1 : -1; 
        int err = dx - dy;
        uint8_t color = (uint8_t)(col & 0xFF);

        while (true) {
            if (x1 >= _vga.vX1() && x1 <= _vga.vX2() && y1 >= _vga.vY1() && y1 <= _vga.vY2()){
                if (_vga.BPP() == 16){
                    _vga.lineBuf16[y1 + _vga.backBufLine][x1] = col;
                } else {
                    _vga.lineBuf8[y1 + _vga.backBufLine][x1] = color;
                }
            }

            if (x1 == x2 && y1 == y2) break;
            int err2 = err * 2;
            if (err2 > -dy) {
                err -= dy;
                x1 += sx;
            }
            if (err2 < dx) {
                err += dx;
                y1 += sy;
            }
        }
    }
}

void GFX::lineAngle(int x, int y, int len, int angle, uint16_t col){
    int x1, y1;
    _vga.LUT(x1, y1, x, y, len, angle);
    line(x, y, x1, y1, col);
}

void GFX::triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint16_t col){
    line(x1, y1, x2, y2, col);
    line(x2, y2, x3, y3, col);
    line(x3, y3, x1, y1, col);
}

void GFX::fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint16_t col){
    // Сортируем вершины по y (y1 <= y2 <= y3)
    if (y1 > y2){ std::swap(y1, y2); std::swap(x1, x2); }
    if (y1 > y3){ std::swap(y1, y3); std::swap(x1, x3); }
    if (y2 > y3){ std::swap(y2, y3); std::swap(x2, x3); }

    auto drawScanline = [&](int y, int xStart, int xEnd){
        hLine(xStart, y, xEnd, col);
    };

    auto edgeInterp = [](int y1, int x1, int y2, int x2, int y) -> int {
        if (y2 == y1) return x1; // избежать деления на 0
        return x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    };

    // Верхняя часть треугольника
    for (int y = y1; y <= y2; y++){
        int xa = edgeInterp(y1, x1, y3, x3, y);
        int xb = edgeInterp(y1, x1, y2, x2, y);
        if (xa > xb) std::swap(xa, xb);
        drawScanline(y, xa, xb);
    }

    // Нижняя часть треугольника
    for (int y = y2 + 1; y <= y3; y++){
        int xa = edgeInterp(y1, x1, y3, x3, y);
        int xb = edgeInterp(y2, x2, y3, x3, y);
        if (xa > xb) std::swap(xa, xb);
        drawScanline(y, xa, xb);
    }
}

void GFX::circle(int xc, int yc, int r, uint16_t col){
    int x = 0;
    int y = r;
    int d = 3 - (r << 1);

    auto drawCircleSegments = [&](int x, int y){
        // Рисуем горизонтальные линии длиной 1 для верх/низ
        putPixel(xc + x, yc + y, col);
        putPixel(xc - x, yc + y, col);
        putPixel(xc + x, yc - y, col);
        putPixel(xc - x, yc - y, col);

        if (x != y){
            putPixel(xc + y, yc + x, col);
            putPixel(xc - y, yc + x, col);
            putPixel(xc + y, yc - x, col);
            putPixel(xc - y, yc - x, col);
        }
    };

    while (y >= x){
        drawCircleSegments(x, y);
        x++;
        if (d > 0){
            y--;
            d = d + ((x - y) << 2) + 10;
        } else {
            d = d + (x << 2) + 6;
        }
    }
}

void GFX::fillCircle(int xc, int yc, int r, uint16_t col){
    int x = 0;
    int y = r;
    int d = 3 - (r << 1);

    // верхняя и нижняя линии по центру
    hLine(xc - r, yc, xc + r, col);

    while (y >= x){
        // Рисуем линии для 4-х сегментов (верх/низ) одной итерацией
        if (x > 0){
            hLine(xc - y, yc + x, xc + y, col);
            hLine(xc - y, yc - x, xc + y, col);
        }
        if (y > x){
            hLine(xc - x, yc + y, xc + x, col);
            hLine(xc - x, yc - y, xc + x, col);
        }

        x++;
        if (d > 0){
            y--;
            d = d + ((x - y) << 2) + 10;
        } else {
            d = d + (x << 2) + 6;
        }
    }
}

void GFX::circleHelper(int xc, int yc, int r, uint8_t corner, uint16_t col){
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    while (y >= x) {
        if (corner & 1) { // верхний левый
            putPixel(xc - x, yc - y, col);
            putPixel(xc - y, yc - x, col);
        }
        if (corner & 2) { // верхний правый
            putPixel(xc + x, yc - y, col);
            putPixel(xc + y, yc - x, col);
        }
        if (corner & 4) { // нижний правый
            putPixel(xc + x, yc + y, col);
            putPixel(xc + y, yc + x, col);
        }
        if (corner & 8) { // нижний левый
            putPixel(xc - x, yc + y, col);
            putPixel(xc - y, yc + x, col);
        }

        x++;
        if (d > 0) {
            y--;
            d += 4 * (x - y) + 10;
        } else {
            d += 4 * x + 6;
        }
    }
}

void GFX::fillCircleHelper(int xc, int yc, int r, uint8_t corner, int ybase, uint16_t col){
    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    while (y >= x) {
        if (corner & 1) { // верхний левый
            hLine(xc - x, yc - y, xc, col);
            hLine(xc - y, yc - x, xc, col);
        }
        if (corner & 2) { // верхний правый
            hLine(xc, yc - y, xc + x, col);
            hLine(xc, yc - x, xc + y, col);
        }
        if (corner & 4) { // нижний правый
            hLine(xc, yc + y, xc + x, col);
            hLine(xc, yc + x, xc + y, col);
        }
        if (corner & 8) { // нижний левый
            hLine(xc - x, yc + y, xc, col);
            hLine(xc - y, yc + x, xc, col);
        }

        x++;
        if (d > 0) {
            y--;
            d += 4 * (x - y) + 10;
        } else {
            d += 4 * x + 6;
        }
    }
}

void GFX::polygon(int x, int y, int radius, int sides, int ang, uint16_t col){
    if (sides < 3) return;

    int* vx = new int[sides];
    int* vy = new int[sides];

    int angleStep = 360 / sides;

    // Вычисляем вершины через LUT
    for (int i = 0; i < sides; i++){
        int angle = ang + i * angleStep;
        _vga.LUT(vx[i], vy[i], x, y, radius, angle);
    }

    // Соединяем вершины линиями
    for (int i = 0; i < sides; i++){
        int x1 = vx[i];
        int y1 = vy[i];
        int x2 = vx[(i + 1) % sides];
        int y2 = vy[(i + 1) % sides];

        line(x1, y1, x2, y2, col); 
    }

    delete[] vx;
    delete[] vy;
}

void GFX::fillPolygon(int x, int y, int radius, int sides, int rotation, uint16_t col){
    if (sides < 3) return;

    int* vx = new int[sides];
    int* vy = new int[sides];

    int angleStep = 360 / sides;

    // Вычисляем вершины через LUT
    for (int i = 0; i < sides; i++){
        int angle = rotation + i * angleStep;
        _vga.LUT(vx[i], vy[i], x, y, radius, angle);
    }

    // Алгоритм scanline для закрашивания
    int yMin = vy[0], yMax = vy[0];
    for (int i = 1; i < sides; i++){
        if (vy[i] < yMin) yMin = vy[i];
        if (vy[i] > yMax) yMax = vy[i];
    }

    for (int yCur = yMin; yCur <= yMax; yCur++){
        // собираем все пересечения текущей строки с ребрами
        int intersections[sides];
        int n = 0;

        for (int i = 0; i < sides; i++){
            int x1 = vx[i], y1 = vy[i];
            int x2 = vx[(i + 1) % sides], y2 = vy[(i + 1) % sides];

            if ((yCur >= y1 && yCur < y2) || (yCur >= y2 && yCur < y1)){
                int xInt = x1 + (yCur - y1) * (x2 - x1) / (y2 - y1);
                intersections[n++] = xInt;
            }
        }

        // сортировка пересечений
        for (int i = 0; i < n-1; i++){
            for (int j = i+1; j < n; j++){
                if (intersections[i] > intersections[j]) std::swap(intersections[i], intersections[j]);
            }
        }

        // рисуем горизонтальные линии между парами пересечений
        for (int i = 0; i < n; i += 2){
            if (i+1 < n){
                hLine(intersections[i], yCur, intersections[i+1], col);
            }
        }
    }

    delete[] vx;
    delete[] vy;
}

void GFX::star(int x, int y, int radius, int sides, int ang, uint16_t col){
    if (sides < 2) return; // минимум 2 "луча"

    int points = sides * 2; // удвоенное количество вершин (внешние+внутренние)
    int* vx = new int[points];
    int* vy = new int[points];

    int angleStep = 360 / points;

    for (int i = 0; i < points; i++){
        int r = (i % 2 == 0) ? radius : radius / 2; // чётные - внешний радиус, нечётные - внутренний
        int angle = ang + i * angleStep;
        _vga.LUT(vx[i], vy[i], x, y, r, angle);
    }

    // соединяем вершины линиями
    for (int i = 0; i < points; i++){
        int x1 = vx[i],     y1 = vy[i];
        int x2 = vx[(i+1)%points], y2 = vy[(i+1)%points];
        line(x1, y1, x2, y2, col);
    }

    delete[] vx;
    delete[] vy;
}

void GFX::fillStar(int x, int y, int radius, int sides, int rotation, uint16_t col){
    if (sides < 2) return;

    int points = sides * 2;
    int* vx = new int[points];
    int* vy = new int[points];

    int angleStep = 360 / points;

    for (int i = 0; i < points; i++){
        int r = (i % 2 == 0) ? radius : radius / 2;
        int angle = rotation + i * angleStep;
        _vga.LUT(vx[i], vy[i], x, y, r, angle);
    }

    // используем тот же scanline, что и в fillPolygon
    int yMin = vy[0], yMax = vy[0];
    for (int i = 1; i < points; i++){
        if (vy[i] < yMin) yMin = vy[i];
        if (vy[i] > yMax) yMax = vy[i];
    }

    for (int yCur = yMin; yCur <= yMax; yCur++){
        int intersections[points];
        int n = 0;

        for (int i = 0; i < points; i++){
            int x1 = vx[i], y1 = vy[i];
            int x2 = vx[(i + 1) % points], y2 = vy[(i + 1) % points];

            if ((yCur >= y1 && yCur < y2) || (yCur >= y2 && yCur < y1)){
                int xInt = x1 + (yCur - y1) * (x2 - x1) / (y2 - y1);
                intersections[n++] = xInt;
            }
        }

        // сортируем пересечения
        for (int i = 0; i < n-1; i++){
            for (int j = i+1; j < n; j++){
                if (intersections[i] > intersections[j])
                    std::swap(intersections[i], intersections[j]);
            }
        }

        // рисуем горизонтальные линии
        for (int i = 0; i < n; i += 2){
            if (i+1 < n){
                hLine(intersections[i], yCur, intersections[i+1], col);
            }
        }
    }

    delete[] vx;
    delete[] vy;
}

// --- Контур звезды с разными радиусами ---
void GFX::star2(int x, int y, int radius1, int radius2, int sides, int rotation, uint16_t col){
    if (sides < 2) return; // минимум 2 луча

    int points = sides * 2;
    int* vx = new int[points];
    int* vy = new int[points];

    int angleStep = 360 / points;

    for (int i = 0; i < points; i++){
        int r = (i % 2 == 0) ? radius1 : radius2; // чётные - внешний радиус, нечётные - внутренний
        int angle = rotation + i * angleStep;
        _vga.LUT(vx[i], vy[i], x, y, r, angle);
    }

    // Соединяем вершины линиями
    for (int i = 0; i < points; i++){
        int x1 = vx[i], y1 = vy[i];
        int x2 = vx[(i + 1) % points], y2 = vy[(i + 1) % points];
        line(x1, y1, x2, y2, col);
    }

    delete[] vx;
    delete[] vy;
}

// --- Заполненная звезда с разными радиусами ---
void GFX::fillStar2(int x, int y, int radius1, int radius2, int sides, int rotation, uint16_t col){
    if (sides < 2) return;

    int points = sides * 2;
    int* vx = new int[points];
    int* vy = new int[points];

    int angleStep = 360 / points;

    for (int i = 0; i < points; i++){
        int r = (i % 2 == 0) ? radius1 : radius2;
        int angle = rotation + i * angleStep;
        _vga.LUT(vx[i], vy[i], x, y, r, angle);
    }

    // Алгоритм scanline
    int yMin = vy[0], yMax = vy[0];
    for (int i = 1; i < points; i++){
        if (vy[i] < yMin) yMin = vy[i];
        if (vy[i] > yMax) yMax = vy[i];
    }

    for (int yCur = yMin; yCur <= yMax; yCur++){
        int intersections[points];
        int n = 0;

        for (int i = 0; i < points; i++){
            int x1 = vx[i], y1 = vy[i];
            int x2 = vx[(i + 1) % points], y2 = vy[(i + 1) % points];

            if ((yCur >= y1 && yCur < y2) || (yCur >= y2 && yCur < y1)){
                int xInt = x1 + (yCur - y1) * (x2 - x1) / (y2 - y1);
                intersections[n++] = xInt;
            }
        }

        // сортировка пересечений
        for (int i = 0; i < n-1; i++){
            for (int j = i+1; j < n; j++){
                if (intersections[i] > intersections[j])
                    std::swap(intersections[i], intersections[j]);
            }
        }

        // рисуем горизонтальные отрезки
        for (int i = 0; i < n; i += 2){
            if (i+1 < n){
                hLine(intersections[i], yCur, intersections[i+1], col);
            }
        }
    }

    delete[] vx;
    delete[] vy;
}

void GFX::ellipse(int xc, int yc, int rx, int ry, uint16_t col){
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int twoRx2 = 2 * rx2;
    int twoRy2 = 2 * ry2;

    int x = 0;
    int y = ry;
    int px = 0;
    int py = twoRx2 * y;

    // Первая часть (dx/dy < 1)
    int p = round(ry2 - (rx2 * ry) + (0.25 * rx2));
    while (px < py) {
        putPixel(xc + x, yc + y, col);
        putPixel(xc - x, yc + y, col);
        putPixel(xc + x, yc - y, col);
        putPixel(xc - x, yc - y, col);

        x++;
        px += twoRy2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= twoRx2;
            p += ry2 + px - py;
        }
    }

    // Вторая часть (dx/dy >= 1)
    p = round(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        putPixel(xc + x, yc + y, col);
        putPixel(xc - x, yc + y, col);
        putPixel(xc + x, yc - y, col);
        putPixel(xc - x, yc - y, col);

        y--;
        py -= twoRx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += twoRy2;
            p += rx2 - py + px;
        }
    }
}

void GFX::fillEllipse(int xc, int yc, int rx, int ry, uint16_t col){
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int twoRx2 = 2 * rx2;
    int twoRy2 = 2 * ry2;

    int x = 0;
    int y = ry;
    int px = 0;
    int py = twoRx2 * y;

    // Первая часть (dx/dy < 1)
    int p = round(ry2 - (rx2 * ry) + (0.25 * rx2));
    while (px < py) {
        hLine(xc - x, yc + y, xc + x, col);
        hLine(xc - x, yc - y, xc + x, col);

        x++;
        px += twoRy2;
        if (p < 0) {
            p += ry2 + px;
        } else {
            y--;
            py -= twoRx2;
            p += ry2 + px - py;
        }
    }

    // Вторая часть (dx/dy >= 1)
    p = round(ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0) {
        hLine(xc - x, yc + y, xc + x, col);
        hLine(xc - x, yc - y, xc + x, col);

        y--;
        py -= twoRx2;
        if (p > 0) {
            p += rx2 - py;
        } else {
            x++;
            px += twoRy2;
            p += rx2 - py + px;
        }
    }
} 

void GFX::arc(int xc, int yc, int r, int angle1, int angle2, uint16_t col){
    if (angle1 > angle2) std::swap(angle1, angle2);
    angle1 %= 360;
    angle2 %= 360;

    int x = 0;
    int y = r;
    int d = 3 - 2 * r;

    auto inAngle = [&](int angle) {
        if (angle1 <= angle2) return (angle >= angle1 && angle <= angle2);
        return (angle >= angle1 || angle <= angle2); // переход через 360
    };

    auto plot = [&](int dx, int dy) {
        // вычисляем угол точки относительно центра
        int angle = (int)(atan2((float)dy, (float)dx) * 180.0 / M_PI);
        if (angle < 0) angle += 360;
        if (inAngle(angle)) putPixel(xc + dx, yc + dy, col);
    };

    while (y >= x) {
        plot(x, y);
        plot(-x, y);
        plot(x, -y);
        plot(-x, -y);
        plot(y, x);
        plot(-y, x);
        plot(y, -x);
        plot(-y, -x);

        x++;
        if (d > 0) {
            y--;
            d += 4 * (x - y) + 10;
        } else {
            d += 4 * x + 6;
        }
    }
} 

void GFX::blur() {
    int width  = _vga.Width();
    int height = _vga.Height();

    if (_vga.BPP() == 16) {
        // ---- RGB565 ----
        uint16_t* buf = PTR_OFFSET_T(_vga.buf, _vga.backBuf, uint16_t);

        for (int y = 0; y < height - 1; y++) {
            uint16_t* line0 = buf + y * width;
            uint16_t* line1 = buf + (y + 1) * width;

            for (int x = 1; x < width - 1; x++) {
                uint16_t c0 = line0[x];
                uint16_t c1 = line1[x];
                uint16_t c2 = line1[x - 1];
                uint16_t c3 = line1[x + 1];

                int r = (R16(c0) + R16(c1) + ((R16(c2) + R16(c3)) >> 1)) / 3;
                int g = (G16(c0) + G16(c1) + ((G16(c2) + G16(c3)) >> 1)) / 3;
                int b = (B16(c0) + B16(c1) + ((B16(c2) + B16(c3)) >> 1)) / 3;

                line0[x] = (r << 11) | (g << 5) | b;
            }
        }

    } else {
        // ---- RGB332 ----
        uint8_t* buf = PTR_OFFSET_T(_vga.buf, _vga.backBuf, uint8_t);

        for (int y = 0; y < height - 1; y++) {
            uint8_t* line0 = buf + y * width;
            uint8_t* line1 = buf + (y + 1) * width;

            for (int x = 1; x < width - 1; x++) {
                uint8_t c0 = line0[x];
                uint8_t c1 = line1[x];
                uint8_t c2 = line1[x - 1];
                uint8_t c3 = line1[x + 1];

                int r = (R8(c0) + R8(c1) + ((R8(c2) + R8(c3)) >> 1)) / 3;
                int g = (G8(c0) + G8(c1) + ((G8(c2) + G8(c3)) >> 1)) / 3;
                int b = (B8(c0) + B8(c1) + ((B8(c2) + B8(c3)) >> 1)) / 3;

                line0[x] = (r << 5) | (g << 2) | b;
            }
        }
    }
}

//============================= Font ==================================
int GFX::stringLength(const char* s){
    int len = 0;
    const uint8_t* p = (const uint8_t*)s;

    while (*p)
    {
        utf8_to_font(p); // просто продвигаем указатель
        len++;
    }
    return len;
}

uint8_t GFX::utf8_to_font(const uint8_t*& p)
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

void GFX::putChar(int x, int y, char ch, uint16_t col){
    const uint8_t* c = font + (ch * 8);

    for (int py = 0; py < 8; py++){
        uint8_t line = *c++;
        int xx = x;

        for (int px = 0; px < 8; px++){
            if (line & (0x80 >> px))
                putPixel(xx, y + py, col);
            xx++;
        }
    }
}

void GFX::putString(int x, int y, TextAlign align, const char* s, uint16_t col){
    int len = stringLength(s);
    int w = len * fontWidth;

    switch (align)
    {
        case CENTER:
            x -= w / 2;
            //y -= fontHeight / 2;
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
        x += fontWidth;
    }
}

void GFX::putText(int cx, int cy, TextAlign align, const char* s, uint16_t col) {
    putString(cx, cy, align, s, col);
}

void GFX::putText(int cx, int cy, TextAlign align, int value, uint16_t col) {
    char buf[16];
    itoa(value, buf, 10);
    putString(cx, cy, align, buf, col);
}

void GFX::putText(int cx, int cy, TextAlign align, float value, uint16_t col) {
    char buf[32];
    dtostrf(value, 0, 2, buf);
    putString(cx, cy, align, buf, col);
}

void GFX::putText(int cx, int cy, TextAlign align, const String& s, uint16_t col) {
    putString(cx, cy, align, s.c_str(), col);
}

