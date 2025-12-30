#include "sprite.h"

Sprite::Sprite(VGA_esp32s3p4& vga) : _vga(vga){

}

Sprite::~Sprite(){
    destroy();
}

void Sprite::destroy(){
    if (buf) { free(buf); buf = nullptr; }
    if (info) { delete[] info; info = nullptr; }
}

bool Sprite::allocateMemory(size_t size){
    if (size <= 0) return false;
    if (buf) {
        free(buf);
        buf = nullptr;
    }

    uint32_t caps = MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM;
    buf = (uint8_t*)heap_caps_malloc(size, caps);
    if (!buf) {
        Serial.println(F("Error: failed to allocate image buffer"));
        return false;
    } else memset(buf, 0, size);

    return true;
}

bool Sprite::createImage(int xx, int yy){
    if (xx <= 0 || yy <= 0) return false;
    destroy();

    frames = 0;
    info = new Image[1];
    memset(info, 0, sizeof(Image));

    bpp = _vga.BPP();
    pixelSize = (bpp == 16) ? 2 : 1;
    info[0].width = xx;
    info[0].height = yy;
    info[0].xx = xx - 1;
    info[0].yy = yy - 1;
    info[0].cx = xx / 2;
    info[0].cy = yy / 2;
    info[0].lineSize = xx * pixelSize;
    info[0].size = info[0].lineSize * yy;
    info[0].offset = 0;
    allocateMemory(info[0].size);

    return true;
}

bool Sprite::getImage(int x1, int y1, int x2, int y2){
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);
    if (x1 > _vga.XX() || y1 > _vga.YY() || x2 < 0 || y2 < 0) return false; 

    x1 = std::max(0, x1);
    x2 = std::min(_vga.XX(), x2);
    y1 = std::max(0, y1);
    y2 = std::min(_vga.YY(), y2);
    
    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;
    createImage(width, height);

    uint8_t* scrBuf = PTR_OFFSET_T(_vga.buf, _vga.backBuf, uint8_t);
    for (int y = 0; y < height; y++){
        uint8_t* src = scrBuf + ((y1 + y) * _vga.Width() + x1) * pixelSize;
        uint8_t* dest = buf + y * info[0].lineSize;
        memcpy(dest, src, info[0].lineSize);
    }

    return true;
}

void Sprite::putImage(int x, int y){
    if (!buf || x > _vga.vX2() || y > _vga.vY2()) return;
    int xx = x + info[0].xx;
    int yy = y + info[0].yy;
    if (xx < _vga.vX1() || yy < _vga.vY1()) return;
    
    int sxl = (x < _vga.vX1() ? (_vga.vX1() - x) : 0);      //Skip X left    
    int sxr = (xx > _vga.vX2() ? (xx - _vga.vX2()) : 0);    //Skip X right
    int syu = (y < _vga.vY1() ? (_vga.vY1() - y) : 0);      //Skip Y up
    int syd = (yy > _vga.vY2() ? (yy - _vga.vY2()) : 0);    //Skip Y down

    int copyX = info[0].lineSize - ((sxr + sxl) << _vga.Shift());
    int copyY = info[0].height - syd - syu;
    if (copyX <= 0 || copyY <= 0) return;

    uint8_t* img = buf + (syu * info[0].lineSize) + (sxl << _vga.Shift());
    uint8_t* scr = (_vga.BPP() == 16)
        ? (uint8_t*)&_vga.lineBuf16[y + _vga.backBufLine + syu][x + sxl]
        : (uint8_t*)&_vga.lineBuf8 [y + _vga.backBufLine + syu][x + sxl];
  
    while(copyY-- > 0){
        memcpy(scr, img, copyX);
        scr += _vga.LineSize();
        img += info[0].lineSize;
    }    
}

void Sprite::putImageScroll(int x, int y, int w, int h, int offsetX, int offsetY, int num) {
    if (!buf || w <= 0 || h <= 0) return;

    const int imgW = info[num].width;
    const int imgH = info[num].height;
    if (imgW <= 0 || imgH <= 0) return;

    const int lineSize = info[num].lineSize;
    uint8_t* imgBase = buf + info[num].offset;

    // Клиппинг области
    int xx = x + w - 1;
    int yy = y + h - 1;
    if (x > _vga.vX2() || y > _vga.vY2() || xx < _vga.vX1() || yy < _vga.vY1()) return;

    int sxl = (x < _vga.vX1()) ? (_vga.vX1() - x) : 0;
    int sxr = (xx > _vga.vX2()) ? (xx - _vga.vX2()) : 0;
    int syu = (y < _vga.vY1()) ? (_vga.vY1() - y) : 0;
    int syd = (yy > _vga.vY2()) ? (yy - _vga.vY2()) : 0;

    int drawW = w - sxl - sxr;
    int drawH = h - syu - syd;
    if (drawW <= 0 || drawH <= 0) return;

    // Нормализуем оффсеты (может быть отрицательными при скроллинге влево/вверх)
    int startX = (offsetX + sxl) % imgW;
    if (startX < 0) startX += imgW;
    int startY = (offsetY + syu) % imgH;
    if (startY < 0) startY += imgH;

    const int pixelSize = 1 << _vga.Shift();           // 1 или 2
    const int skipBytes = _vga.Width() * pixelSize;    // переход на следующую строку экрана
    const int drawBytes = drawW * pixelSize;

    uint8_t* scr = (_vga.BPP() == 16)
        ? (uint8_t*)&_vga.lineBuf16[y + syu + _vga.backBufLine][x + sxl]
        : (uint8_t*)&_vga.lineBuf8 [y + syu + _vga.backBufLine][x + sxl];

    // Рисуем каждую видимую строку
    for (int jy = 0; jy < drawH; jy++) {
        int srcY = (startY + jy) % imgH;
        uint8_t* srcLine = imgBase + srcY * lineSize;

        uint8_t* dst = scr + jy * skipBytes;

        int copied = 0;
        int posX = startX;

        // Копируем по частям с wrap по X
        while (copied < drawW) {
            int chunkW = imgW - posX;
            if (chunkW > drawW - copied) chunkW = drawW - copied;

            memcpy(dst + copied * pixelSize, srcLine + posX * pixelSize, chunkW * pixelSize);

            copied += chunkW;
            posX = 0;  // следующий кусок с начала строки изображения
        }
    }
}

void Sprite::putSpriteScroll(int x, int y, int w, int h, uint16_t maskColor, int offsetX, int offsetY, int num) {
    if (!buf || w <= 0 || h <= 0) return;

    const int imgW = info[num].width;
    const int imgH = info[num].height;
    if (imgW <= 0 || imgH <= 0) return;

    const int lineSize = info[num].lineSize;
    uint8_t* imgBase = buf + info[num].offset;

    // Клиппинг
    int xx = x + w - 1;
    int yy = y + h - 1;
    if (x > _vga.vX2() || y > _vga.vY2() || xx < _vga.vX1() || yy < _vga.vY1()) return;

    int sxl = (x < _vga.vX1()) ? (_vga.vX1() - x) : 0;
    int sxr = (xx > _vga.vX2()) ? (xx - _vga.vX2()) : 0;
    int syu = (y < _vga.vY1()) ? (_vga.vY1() - y) : 0;
    int syd = (yy > _vga.vY2()) ? (yy - _vga.vY2()) : 0;

    int drawW = w - sxl - sxr;
    int drawH = h - syu - syd;
    if (drawW <= 0 || drawH <= 0) return;

    // Нормализация оффсетов
    int startX = (offsetX + sxl) % imgW;
    if (startX < 0) startX += imgW;
    int startY = (offsetY + syu) % imgH;
    if (startY < 0) startY += imgH;

    const int pixelSize = 1 << _vga.Shift();  // 1 или 2 байта
    const int skipBytes = _vga.Width() * pixelSize;

    uint8_t* scr = (_vga.BPP() == 16)
        ? (uint8_t*)&_vga.lineBuf16[y + syu + _vga.backBufLine][x + sxl]
        : (uint8_t*)&_vga.lineBuf8 [y + syu + _vga.backBufLine][x + sxl];

    // Если маска не нужна (maskColor = 0xFFFF или другой флаг), можно ускорить — но пока оставим полную версию

    for (int jy = 0; jy < drawH; jy++) {
        int srcY = (startY + jy) % imgH;
        uint8_t* srcLine = imgBase + srcY * lineSize;

        uint8_t* dst = scr + jy * skipBytes;

        int posX = startX;
        int copied = 0;

        while (copied < drawW) {
            int chunkW = imgW - posX;
            if (chunkW > drawW - copied) chunkW = drawW - copied;

            if (_vga.BPP() == 16) {
                uint16_t* dst16 = (uint16_t*)dst;
                uint16_t* src16 = (uint16_t*)(srcLine + posX * 2);
                uint16_t mask16 = maskColor;

                for (int i = 0; i < chunkW; i++) {
                    uint16_t pixel = src16[i];
                    if (pixel != mask16) {
                        dst16[copied + i] = pixel;
                    }
                }
            } else {
                uint8_t* dst8 = dst;
                uint8_t* src8 = srcLine + posX;
                uint8_t mask8 = (uint8_t)maskColor;

                for (int i = 0; i < chunkW; i++) {
                    uint8_t pixel = src8[i];
                    if (pixel != mask8) {
                        dst8[copied + i] = pixel;
                    }
                }
            }

            copied += chunkW;
            posX = 0;
        }
    }
}

// zoomX_fp, zoomY_fp: 16.16 fixed (1.0 = 65536)
void Sprite::putImageZoomNN(int x, int y, uint32_t zoomX_fp, uint32_t zoomY_fp){
    if (!buf || zoomX_fp == 0 || zoomY_fp == 0) return;

    const int srcW = info[0].width;
    const int srcH = info[0].height;
    const int srcLine = info[0].lineSize;   // bytes per row in sprite buffer

    // Destination size
    const int dstW = (int)(((uint64_t)srcW * zoomX_fp) >> 16);
    const int dstH = (int)(((uint64_t)srcH * zoomY_fp) >> 16);
    if (dstW <= 0 || dstH <= 0) return;

    // Anchor offsets (как у тебя)
    int dx0 = x + info[0].xx;
    int dy0 = y + info[0].yy;
    int dx1 = dx0 + dstW - 1;
    int dy1 = dy0 + dstH - 1;

    // Quick reject
    if (dx0 > _vga.vX2() || dy0 > _vga.vY2() || dx1 < _vga.vX1() || dy1 < _vga.vY1()) return;

    // Clip destination rect to viewport
    const int clipL = _vga.vX1();
    const int clipT = _vga.vY1();
    const int clipR = _vga.vX2();
    const int clipB = _vga.vY2();

    int drawL = (dx0 < clipL) ? clipL : dx0;
    int drawT = (dy0 < clipT) ? clipT : dy0;
    int drawR = (dx1 > clipR) ? clipR : dx1;
    int drawB = (dy1 > clipB) ? clipB : dy1;

    const int drawW = drawR - drawL + 1;
    const int drawH = drawB - drawT + 1;
    if (drawW <= 0 || drawH <= 0) return;

    // Inverse zoom (чтобы деления были 1 раз, а не в каждом пикселе)
    const uint32_t invZoomX_fp = (uint32_t)(((uint64_t)1 << 32) / zoomX_fp);
    const uint32_t invZoomY_fp = (uint32_t)(((uint64_t)1 << 32) / zoomY_fp);

    // Source start (16.16): src = (dest - d0) / zoom
    uint32_t srcX0_fp = (uint32_t)((uint64_t)(drawL - dx0) * invZoomX_fp >> 16);
    uint32_t srcY_fp  = (uint32_t)((uint64_t)(drawT - dy0) * invZoomY_fp >> 16);

    // Step per 1 dest pixel (16.16)
    const uint32_t stepX_fp = (uint32_t)(invZoomX_fp >> 16);
    const uint32_t stepY_fp = (uint32_t)(invZoomY_fp >> 16);

    // Destination base pointer
    uint8_t* dstBase = (_vga.BPP() == 16)
        ? (uint8_t*)&_vga.lineBuf16[drawT + _vga.backBufLine][drawL]
        : (uint8_t*)&_vga.lineBuf8 [drawT + _vga.backBufLine][drawL];

    const uint8_t* srcBase = buf;

    if (_vga.BPP() == 16)
    {
        for (int yy = 0; yy < drawH; yy++)
        {
            int sy = (int)(srcY_fp >> 16);
            if (sy < 0) sy = 0; else if (sy >= srcH) sy = srcH - 1;

            const uint16_t* srow = (const uint16_t*)(srcBase + sy * srcLine);
            uint16_t* drow = (uint16_t*)(dstBase + yy * _vga.LineSize());

            uint32_t sx_fp = srcX0_fp;
            for (int xx = 0; xx < drawW; xx++)
            {
                int sx = (int)(sx_fp >> 16);
                if (sx < 0) sx = 0; else if (sx >= srcW) sx = srcW - 1;

                drow[xx] = srow[sx];
                sx_fp += stepX_fp;
            }
            srcY_fp += stepY_fp;
        }
    }
    else // 8bpp
    {
        for (int yy = 0; yy < drawH; yy++)
        {
            int sy = (int)(srcY_fp >> 16);
            if (sy < 0) sy = 0; else if (sy >= srcH) sy = srcH - 1;

            const uint8_t* srow = (const uint8_t*)(srcBase + sy * srcLine);
            uint8_t* drow = (uint8_t*)(dstBase + yy * _vga.LineSize());

            uint32_t sx_fp = srcX0_fp;
            for (int xx = 0; xx < drawW; xx++)
            {
                int sx = (int)(sx_fp >> 16);
                if (sx < 0) sx = 0; else if (sx >= srcW) sx = srcW - 1;

                drow[xx] = srow[sx];
                sx_fp += stepX_fp;
            }
            srcY_fp += stepY_fp;
        }
    }
}

/*
void Sprite::putImageZoom(int x, int y, int zX, int zY, int num){
    if (!buf || zX <= 0 || zY <= 0) return;

    int xx = x + zX - 1;
    int yy = y + zY - 1;
    if (x > _vga.vX2() || y > _vga.vY2()) return;
    if (xx < _vga.vX1() || yy < _vga.vY1()) return;

    const int imgW = info[num].width;
    const int imgH = info[num].height;
    const int lineSize = info[num].lineSize;
    uint8_t* imgBase = buf + info[num].offset;

    int sxl = (x < _vga.vX1()) ? (_vga.vX1() - x) : 0;
    int sxr = (xx > _vga.vX2()) ? (xx - _vga.vX2()) : 0;
    int syu = (y < _vga.vY1()) ? (_vga.vY1() - y) : 0;
    int syd = (yy > _vga.vY2()) ? (yy - _vga.vY2()) : 0;

    const int outW = zX - sxl - sxr;
    const int outH = zY - syu - syd;
    if (outW <= 0 || outH <= 0) return;

    // tx/ty считаем в "выходных координатах" (0..outW-1)
    // но формула использует глобальные i/j (с учётом сдвига sxl/syu)
    for (int ii = 0; ii < outW; ii++){
        int i = ii + sxl;
        tx[ii] = (imgW * i) / zX;
    }

    uint8_t* scr = (uint8_t*)&_vga.lineBuf8[y + _vga.backBufLine + syu][x + sxl];
    const int dstStride = _vga.Width();

    int prevSrcY = -1;
    uint8_t* prevLine = nullptr;

    for (int jj = 0; jj < outH; jj++){
        int j = jj + syu;
        int srcY = (imgH * j) / zY;

        uint8_t* dstLine = scr + jj * dstStride;

        if (srcY == prevSrcY){
            memcpy(dstLine, prevLine, outW);
            continue;
        }

        uint8_t* srcLine = imgBase + srcY * lineSize;

        for (int ii = 0; ii < outW; ii++){
            dstLine[ii] = srcLine[ tx[ii] ];
        }

        prevSrcY = srcY;
        prevLine = dstLine;
    }
}

*/
void Sprite::putImageZoom(int x, int y, int zX, int zY, int num){
    if (!buf || x > _vga.vX2() || y > _vga.vY2() || zX <= 0 || zY <= 0) return;
    int xx = x + zX - 1;
    int yy = y + zY - 1;
    if (xx < _vga.vX1() || yy < _vga.vY1()) return;

    int sxl = (x  < _vga.vX1()) ? (_vga.vX1() - x)  : 0;
    int sxr = (xx > _vga.vX2()) ? (xx - _vga.vX2()) : 0;
    int syu = (y  < _vga.vY1()) ? (_vga.vY1() - y)  : 0;
    int syd = (yy > _vga.vY2()) ? (yy - _vga.vY2()) : 0;

    int drawW = zX - sxl - sxr;
    int drawH = zY - syu - syd;
    if (drawW <= 0 || drawH <= 0) return;

    const int imgW = info[num].width;
    for (int i = 0; i < drawW; i++) tx[i] = (imgW * (i + sxl)) / zX;

    const int imgH = info[num].height;
    for (int j = 0; j < drawH; j++) ty[j] = (imgH * (j + syu)) / zY;

    const int imgLineSize = info[num].lineSize;
    const int skip = _vga.Width();
    const int skipScr =  skip - drawW;
    
    if (_vga.BPP() == 16){
        uint16_t *scr = _vga.lineBuf16[y + _vga.backBufLine + syu] + x + sxl;
        uint8_t  *imgBase = buf + info[num].offset;

        int drawW2X = drawW << 1;
        int j = 0;
        while (j < drawH) {
            uint16_t *lineStart = scr;
            int ty0 = ty[j];

            // строка исходника (16bpp = 2 байта на пиксель)
            uint16_t *img = (uint16_t*)(imgBase + (ty0 * imgLineSize));

            for (int i = 0; i < drawW; i++) {
                *scr++ = img[ tx[i] ];
            }

            // переход на следующую строку экрана
            scr += skipScr;
            j++;

            // копируем строку, пока ty не меняется
            while (j < drawH && ty[j] == ty0) {
                memcpy(scr, lineStart, drawW2X); // 2 байта на пиксель
                scr += skip;
                j++;
            }
        }
    } else {
        uint8_t *scr = _vga.lineBuf8[y + _vga.backBufLine + syu] + x + sxl;
        uint8_t *imgBase = buf + info[num].offset;

        int j = 0;
        while (j < drawH) {
            uint8_t *lineStart = scr;
            int ty0 = ty[j];

            // правильная строка исходника
            uint8_t *img = imgBase + (ty0 * imgLineSize);

            for (int i = 0; i < drawW; i++) {
                *scr++ = img[ tx[i] ];
            }

            // переход на следующую строку экрана
            scr += skipScr;
            j++;

            // копируем строку, пока ty не меняется
            while (j < drawH && ty[j] == ty0) {
                memcpy(scr, lineStart, drawW);
                scr += skip;
                j++;
            }
        }
    }       
}

void Sprite::putSpriteZoom(int x, int y, int zX, int zY, uint16_t maskColor, int num){
    if (!buf || x > _vga.vX2() || y > _vga.vY2() || zX <= 0 || zY <= 0) return;
    int xx = x + zX - 1;
    int yy = y + zY - 1;
    if (xx < _vga.vX1() || yy < _vga.vY1()) return;

    int sxl = (x  < _vga.vX1()) ? (_vga.vX1() - x)  : 0;
    int sxr = (xx > _vga.vX2()) ? (xx - _vga.vX2()) : 0;
    int syu = (y  < _vga.vY1()) ? (_vga.vY1() - y)  : 0;
    int syd = (yy > _vga.vY2()) ? (yy - _vga.vY2()) : 0;

    int drawW = zX - sxl - sxr;
    int drawH = zY - syu - syd;
    if (drawW <= 0 || drawH <= 0) return;

    const int imgW = info[num].width;
    for (int i = 0; i < drawW; i++) tx[i] = (imgW * (i + sxl)) / zX;

    const int imgH = info[num].height;
    for (int j = 0; j < drawH; j++) ty[j] = (imgH * (j + syu)) / zY;

    const int imgLineSize = info[num].lineSize;
    const int skip = _vga.Width();          // ширина экрана в ПИКСЕЛЯХ
    const int skipScr = skip - drawW;

    uint8_t *imgBase = buf + info[num].offset;

    if (_vga.BPP() == 16) {
        uint16_t *scr = _vga.lineBuf16[y + _vga.backBufLine + syu] + x + sxl;
        uint16_t mask = (uint16_t)maskColor;

        int j = 0;
        while (j < drawH) {
            uint16_t *lineStart = scr;
            int ty0 = ty[j];

            uint16_t *img = (uint16_t*)(imgBase + (ty0 * imgLineSize));

            bool hasMask = false;

            for (int i = 0; i < drawW; i++) {
                uint16_t p = img[ tx[i] ];
                if (p != mask) *scr = p;
                else hasMask = true;
                scr++;
            }

            scr += skipScr;
            j++;

            if (!hasMask) {
                while (j < drawH && ty[j] == ty0) {
                    memcpy(scr, lineStart, (size_t)drawW * 2);
                    scr += skip;
                    j++;
                }
            } else {
                while (j < drawH && ty[j] == ty0) {
                    uint16_t *scrLine = scr;
                    for (int i = 0; i < drawW; i++) {
                        uint16_t p = img[ tx[i] ];
                        if (p != mask) scrLine[i] = p;
                    }
                    scr += skip;
                    j++;
                }
            }
        }
    } else {
        uint8_t *scr = _vga.lineBuf8[y + _vga.backBufLine + syu] + x + sxl;
        uint8_t mask = (uint8_t)maskColor;

        int j = 0;
        while (j < drawH) {
            uint8_t *lineStart = scr;
            int ty0 = ty[j];

            uint8_t *img = imgBase + (ty0 * imgLineSize);

            bool hasMask = false;

            for (int i = 0; i < drawW; i++) {
                uint8_t p = img[ tx[i] ];
                if (p != mask) *scr = p;
                else hasMask = true;
                scr++;
            }

            scr += skipScr;
            j++;

            if (!hasMask) {
                while (j < drawH && ty[j] == ty0) {
                    memcpy(scr, lineStart, (size_t)drawW);
                    scr += skip;
                    j++;
                }
            } else {
                while (j < drawH && ty[j] == ty0) {
                    uint8_t *scrLine = scr;
                    for (int i = 0; i < drawW; i++) {
                        uint8_t p = img[ tx[i] ];
                        if (p != mask) scrLine[i] = p;
                    }
                    scr += skip;
                    j++;
                }
            }
        }
    }
}

void Sprite::putImage(int x, int y, int num){
    if (!buf || x > _vga.vX2() || y > _vga.vY2() || num < 0 || num > frames) return;

    int xx = x + info[num].xx;
    int yy = y + info[num].yy;
    if (xx < _vga.vX1() || yy < _vga.vY1()) return;
    
    int sxl = (x < _vga.vX1() ? (_vga.vX1() - x) : 0);      //Skip X left    
    int sxr = (xx > _vga.vX2() ? (xx - _vga.vX2()) : 0);    //Skip X right
    int syu = (y < _vga.vY1() ? (_vga.vY1() - y) : 0);      //Skip Y up
    int syd = (yy > _vga.vY2() ? (yy - _vga.vY2()) : 0);    //Skip Y down

    int copyX = info[num].lineSize - ((sxr + sxl) << _vga.Shift());
    int copyY = info[num].height - syd - syu;
    if (copyX <= 0 || copyY <= 0) return;

    uint8_t* img = buf + (syu * info[num].lineSize) + (sxl << _vga.Shift()) + info[num].offset;
    uint8_t* scr = (_vga.BPP() == 16)
        ? (uint8_t*)&_vga.lineBuf16[y + _vga.backBufLine + syu][x + sxl]
        : (uint8_t*)&_vga.lineBuf8 [y + _vga.backBufLine + syu][x + sxl];
  
    while(copyY-- > 0){
        memcpy(scr, img, copyX);
        scr += _vga.LineSize();
        img += info[num].lineSize;
    }  
}

void Sprite::putSprite(int x, int y, uint16_t maskColor){
    if (!buf || x > _vga.vX2() || y > _vga.vY2()) return;

    int xx = x + info[0].xx;
    int yy = y + info[0].yy;
    if (xx < _vga.vX1() || yy < _vga.vY1()) return;
    
    int sxl = (x < _vga.vX1()   ? (_vga.vX1() - x)  : 0);   //Skip X left    
    int sxr = (xx > _vga.vX2()  ? (xx - _vga.vX2()) : 0);   //Skip X right
    int syu = (y < _vga.vY1()   ? (_vga.vY1() - y)  : 0);   //Skip Y up
    int syd = (yy > _vga.vY2()  ? (yy - _vga.vY2()) : 0);   //Skip Y down

    int copyX = info[0].width - sxr - sxl;
    int copyY = info[0].height - syd - syu;
    if (copyX <= 0 || copyY <= 0) return;

    int skipScr = _vga.Width() - copyX;
    int skipSour = info[0].width - copyX;

    if (_vga.BPP() == 16) {
        uint16_t* img = (uint16_t*)(buf + (syu * info[0].lineSize) + (sxl << _vga.Shift()));
        uint16_t* scr = &_vga.lineBuf16[_vga.backBufLine + y + syu][x + sxl];
  
        while(copyY-- > 0){
            for (int i = 0; i < copyX; i++){
                if (*img != maskColor) *scr = *img;
                img++;
                scr++;
            }

            scr += skipScr;
            img += skipSour;
        }  
    } else {
        uint8_t col = (uint8_t)(maskColor & 0xFF);
        uint8_t* img = buf + (syu * info[0].lineSize) + sxl;
        uint8_t* scr = &_vga.lineBuf8[_vga.backBufLine + y + syu][x + sxl];
  
        while(copyY-- > 0){
            for (int i = 0; i < copyX; i++){
                if (*img != col) *scr = *img;
                img++;
                scr++;
            }

            scr += skipScr;
            img += skipSour;
        }  
    }
}

void Sprite::putSprite(int x, int y, uint16_t maskColor, int num){
    if (!buf || x > _vga.vX2() || y > _vga.vY2() || num < 0 || num > frames) return;

    int xx = x + info[num].xx;
    int yy = y + info[num].yy;
    if (xx < _vga.vX1() || yy < _vga.vY1()) return;
    
    int sxl = (x < _vga.vX1()   ? (_vga.vX1() - x)  : 0);   //Skip X left    
    int sxr = (xx > _vga.vX2()  ? (xx - _vga.vX2()) : 0);   //Skip X right
    int syu = (y < _vga.vY1()   ? (_vga.vY1() - y)  : 0);   //Skip Y up
    int syd = (yy > _vga.vY2()  ? (yy - _vga.vY2()) : 0);   //Skip Y down

    int copyX = info[num].width - sxr - sxl;
    int copyY = info[num].height - syd - syu;
    if (copyX <= 0 || copyY <= 0) return;

    int skipScr = _vga.Width() - copyX;
    int skipSour = info[num].width - copyX;

    if (_vga.BPP() == 16) {
        uint16_t* img = (uint16_t*)(buf + info[num].offset + (syu * info[num].lineSize) + (sxl << _vga.Shift()));
        uint16_t* scr = &_vga.lineBuf16[_vga.backBufLine + y + syu][x + sxl];
  
        while(copyY-- > 0){
            for (int i = 0; i < copyX; i++){
                if (*img != maskColor) *scr = *img;
                img++;
                scr++;
            }

            scr += skipScr;
            img += skipSour;
        }  
    } else {
        uint8_t col = (uint8_t)(maskColor & 0xFF);
        uint8_t* img = buf + info[num].offset + (syu * info[num].lineSize) + (sxl << _vga.Shift());
        uint8_t* scr = &_vga.lineBuf8[_vga.backBufLine + y + syu][x + sxl];
  
        while(copyY-- > 0){
            for (int i = 0; i < copyX; i++){
                if (*img != col) *scr = *img;
                img++;
                scr++;
            }

            scr += skipScr;
            img += skipSour;
        }  
    }
}

bool Sprite::loadSprite(const uint8_t* data){
    if (!data) return false;

    bpp = *data++;
    if (bpp != 8 && bpp != 16) return false;
    frames = *data++;
    int count = frames + 1; // + основной кадр

    if (info) destroy();
    info = new Image[count];
    memset(info, 0, sizeof(Image) * count);
    Serial.printf("Loading sprite: BPP - %d, Frames - %d\n", bpp, count);

    int fullSize = 0;
    data += count * 4; // пропускаем оффсеты
    uint8_t *ptr = (uint8_t*)data;

    for (int i = 0; i < count; i++) {
        info[i].width  = (*data++) | (*data++ << 8);
        info[i].height = (*data++) | (*data++ << 8);        
        info[i].xx     = info[i].width - 1;
        info[i].yy     = info[i].height - 1;
        info[i].cx     = info[i].width / 2;
        info[i].cy     = info[i].height / 2;

        int lineSize = info[i].width * (bpp == 16 ? 2 : 1);
        int size = info[i].height * lineSize;

        info[i].lineSize = info[i].width * (_vga.BPP() == 16 ? 2 : 1);
        info[i].size = info[i].height * info[i].lineSize;
        info[i].offset = (i == 0) ? 0 : (info[i - 1].offset + info[i - 1].size);

        Serial.printf("Loading frame %d: Width - %d, Height - %d, lineSize: %d, Size: %d\n", i, info[i].width, info[i].height, info[i].lineSize, info[i].size);
        Serial.printf("offset: %d\n", info[i].offset);
        data += size;
        fullSize += info[i].size;
    }
    Serial.printf("Total sprite size: %d bytes\n", fullSize);

    allocateMemory(fullSize);
    for (int i = 0; i < count; i++) {
        ptr += 4; // пропускаем ширину и высоту

        if (bpp == _vga.BPP()){

            memcpy(buf + info[i].offset, ptr, info[i].size);

        } else if (bpp == 16 && _vga.BPP() == 8){ 

            int pixels = info[i].width * info[i].height;
            for (int ii = 0; ii < pixels; ii++){
                int j = ii * 2;
                uint16_t col16 = (uint16_t)ptr[j] | ((uint16_t)ptr[j + 1] << 8);

                uint8_t rr = (col16 >> 13) & 0x07;
                uint8_t gg = (col16 >> 8)  & 0x07;
                uint8_t bb = (col16 >> 3) & 0x03;

                buf[info[i].offset + ii] = (rr << 5) | (gg << 2) | bb;
            }

        } else {
            int pixels = info[i].width * info[i].height;

            // источник 8bpp
            const uint8_t* src8 = ptr;

            // назначение 16bpp (буфер у тебя uint8_t*, поэтому кастим)
            uint16_t* dst16 = (uint16_t*)(buf + info[i].offset);

            for (int ii = 0; ii < pixels; ii++){
                uint8_t c = src8[ii];

                uint8_t r3 = (c >> 5) & 0x07;
                uint8_t g3 = (c >> 2) & 0x07;
                uint8_t b2 =  c       & 0x03;

                uint16_t r5 = (uint16_t)((r3 << 2) | (r3 >> 1));
                uint16_t g6 = (uint16_t)((g3 << 3) | (g3));
                uint16_t b5 = (uint16_t)((b2 << 3) | (b2 << 1) | (b2 >> 1));

                dst16[ii] = (r5 << 11) | (g6 << 5) | b5;
            }
        }

        int srcLineSize = info[i].width * (bpp == 16 ? 2 : 1);
        int srcSize     = info[i].height * srcLineSize;
        ptr += srcSize;   
    }

    Serial.printf("Loading sprite: BPP - %d, Frames - %d\n", bpp, frames);
    for (int i = 0; i < count; i++) Serial.printf("Frame %d: Offset - %d, Size - %d bytes\n", i, info[i].offset, info[i].size);
  
    return true;
}

bool Sprite::loadSprite(const uint8_t* data, int num){
    Serial.printf("\nLoading single frame %d from sprite\n", num);

    if (!data || num < 0) return false;

    bpp = *data++;
    if (bpp != 8 && bpp != 16) return false;
    frames = *data++;
    if (num > frames) return false;
    int count = frames + 1;
    int tmp = count - num - 1;
    Serial.printf("bpp: %d, frames: %d, count: %d\n", bpp, frames, count);

    if (info) destroy();
    info = new Image[count];
    memset(info, 0, sizeof(Image));

    data += num * 4; // пропускаем оффсеты
    uint32_t offset = (*data++) | (*data++ << 8) | (*data++ << 16) | (*data++ << 24);
    Serial.printf("Frame %d offset: %d\n", num, offset);
    data += tmp * 4; // пропускаем остальные оффсеты
    data += offset;

    info[0].width  = (*data++) | (*data++ << 8);
    info[0].height = (*data++) | (*data++ << 8); 
    Serial.printf("Loading frame %d: Width - %d, Height - %d\n", num, info[0].width, info[0].height);       
    info[0].xx = info[0].width - 1;
    info[0].yy = info[0].height - 1;
    info[0].cx = info[0].width / 2;
    info[0].cy = info[0].height / 2;   
    int lineSize = info[0].width * (bpp == 16 ? 2 : 1);
    int size = info[0].height * lineSize;
    info[0].lineSize = info[0].width * (_vga.BPP() == 16 ? 2 : 1);
    info[0].size = info[0].height * info[0].lineSize;

    allocateMemory(info[0].size);
    if (bpp == _vga.BPP()){
        memcpy(buf, data, info[0].size);
    } else if (bpp == 16 && _vga.BPP() == 8){
        Serial.println("Converting 16bpp to 8bpp");

        int pixels = info[0].width * info[0].height;
        for (int ii = 0; ii < pixels; ii++){
            int j = ii * 2;

            uint16_t col16 = (uint16_t)data[j] | ((uint16_t)data[j + 1] << 8);
            uint8_t rr = (col16 >> 13) & 0x07;
            uint8_t gg = (col16 >> 8)  & 0x07;
            uint8_t bb = (col16 >> 3) & 0x03;

            buf[ii] = (rr << 5) | (gg << 2) | bb;
        }        
    } else {
        Serial.println("Converting 8bpp to 16bpp");

        int pixels = info[0].width * info[0].height;
        uint16_t* dst16 = (uint16_t*)buf;

        for (int ii = 0; ii < pixels; ii++){
            uint8_t c = data[ii];

            uint8_t r3 = (c >> 5) & 0x07;
            uint8_t g3 = (c >> 2) & 0x07;
            uint8_t b2 =  c       & 0x03;
            uint16_t r5 = (uint16_t)((r3 << 2) | (r3 >> 1));
            uint16_t g6 = (uint16_t)((g3 << 3) | (g3));
            uint16_t b5 = (uint16_t)((b2 << 3) | (b2 << 1) | (b2 >> 1));

            dst16[ii] = (r5 << 11) | (g6 << 5) | b5;
        }        
    } 
    
    return true;
}
