#pragma once

#include <Arduino.h>
#include "..\VGA_esp32s3p4.h"

typedef struct{
    int width, height;
    int size;
} MapStruct;

class Tiles{
    public:
        uint8_t *mapData = nullptr; 
        uint8_t *imgData = nullptr;

        int MapWidth()    {return map.width;};
        int MapHeight()   {return map.height;};
        int MapSize()     {return map.size;};

        Tiles(VGA_esp32s3p4 &vga);
        ~Tiles();

        bool loadMap(const uint8_t *data);
        bool loadTiles(const uint8_t *data, int num);        
        void drawPixelMap(int x, int y);

    protected:
        MapStruct map;    

        bool allocateMemoryMap(size_t size);

        VGA_esp32s3p4 &_vga;
};