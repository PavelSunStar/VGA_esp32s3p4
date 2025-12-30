#pragma once

#include <Arduino.h>
#include "..\VGA_esp32s3p4.h"

#define MAX_IMAGES 256

typedef struct {
    int width, height;
    int xx, yy;
    int cx, cy;    
    int lineSize, size;
    uint32_t offset;
} Image;

class Sprite{
    public:
        uint8_t *buf = nullptr; // данные картинок в одном буфере

        Sprite(VGA_esp32s3p4& vga);
        ~Sprite();

        int MaxFrame(){return frames;};
        int Frames(){return frames + 1;}; // + основной кадр
        int Width(int num){ return (info && num >= 0 && num <= frames) ? info[num].width : 0;};
        int Height(int num){ return (info && num >= 0 && num <= frames) ? info[num].height : 0;};
        int XX(int num){ return (info && num >= 0 && num <= frames) ? info[num].xx : 0;};
        int YY(int num){ return (info && num >= 0 && num <= frames) ? info[num].yy : 0;};        
        int CX(int num){ return (info && num >= 0 && num <= frames) ? info[num].cx : 0;};
        int CY(int num){ return (info && num >= 0 && num <= frames) ? info[num].cy : 0;};
        int Offset(int num){ return (info && num >= 0 && num <= frames) ? info[num].offset : 0;};
        int LineSize(int num){ return (info && num >= 0 && num <= frames) ? info[num].lineSize : 0;};

        bool getImage(int x1, int y1, int x2, int y2);
        void putImage(int x, int y);
        void putImage(int x, int y, int num); 
        void putImageScroll(int x, int y, int w, int h, int offsetX, int offsetY, int num);
        void putSpriteScroll(int x, int y, int w, int h, uint16_t maskColor, int offsetX, int offsetY, int num);       
        void putImageZoom(int x, int y, int zX, int zY, int num);
        //void putImageZoom(int x, int y, int zX, int zY);

        void putImageZoomNN(int x, int y, uint32_t zoomX_fp, uint32_t zoomY_fp);
        void putSprite(int x, int y, uint16_t maskColor);
        void putSprite(int x, int y, uint16_t maskColor, int num);
        void putSpriteZoom(int x, int y, int zX, int zY, uint16_t maskColor, int num);
        bool loadSprite(const uint8_t* data);
        bool loadSprite(const uint8_t* data, int num);

    protected:  
        int bpp, pixelSize;     // бит на пиксель (8 или 16)
        uint8_t frames = 0;     // количество кадров / изображений
        Image   *info;          // динамический массив структур Image
        int tx[1024];           // временные переменные для масштабирования
        int ty[1024];

        void destroy();
        bool createImage(int xx, int yy);
        bool allocateMemory(size_t size); 

        VGA_esp32s3p4& _vga;        
};

/*
void imageZoom(int x, int y, int dstW, int dstH) {
  int xx[dstW];
  for (int i = 0; i < dstW; i++) 
    xx[i] = i * sp.Width(0) / dstW; 
  int yy[dstH];
  for (int i = 0; i < dstH; i++) 
    yy[i] = i * sp.Height(0) / dstH; 

  for (int j = 0; j < dstH; j++)
    for (int i = 0; i < dstW; i++){
      int col = sp.buf[yy[j] * sp.Width(0) + xx[i]];
      gfx.putPixel(x + i, y + j, col);
  }  
}
*/

