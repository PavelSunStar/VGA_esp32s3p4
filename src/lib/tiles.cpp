#include "Tiles.h"

Tiles::Tiles(VGA_esp32s3p4 &vga) : _vga(vga) {

}

Tiles::~Tiles() {

}

bool Tiles::allocateMemoryMap(size_t size){
    if (size <= 0) return false;
    if (mapData) {
        free(mapData);
        mapData = nullptr;
    }

    uint32_t caps = MALLOC_CAP_8BIT | MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM;
    mapData = (uint8_t*)heap_caps_malloc(size, caps);
    if (!mapData) {
        Serial.println(F("Error: failed to allocate map buffer"));
        return false;
    } else memset(mapData, 0, size);

    return true;
}

bool Tiles::loadMap(const uint8_t *data) {
    if (!data) return false;
    map.width = (int)(*data++) | ((int)(*data++) << 8);
    map.height = (int)(*data++) | ((int)(*data++) << 8);
    map.size = map.width * map.height;

    if (!allocateMemoryMap(map.size)) return false;
    memcpy(mapData, data, map.size);

    Serial.println("Map loaded successfully");
    return true;
}    

void Tiles::drawPixelMap(int x, int y){
    if (!mapData || x > _vga.vX2() || y > _vga.vY2()) return;
    
    int xx = x + map.width - 1;
    int yy = y + map.height - 1;
    if (xx < _vga.vX1() || yy < _vga.vY1()) return;

    int sxl = (x < _vga.vX1() ? (_vga.vX1() - x) : 0);      //Skip X left    
    int sxr = (xx > _vga.vX2() ? (xx - _vga.vX2()) : 0);    //Skip X right
    int syu = (y < _vga.vY1() ? (_vga.vY1() - y) : 0);      //Skip Y up
    int syd = (yy > _vga.vY2() ? (yy - _vga.vY2()) : 0);    //Skip Y down

    int copyX = map.width - sxr - sxl;
    int copyY = map.height - syd - syu;
    if (copyX <= 0 || copyY <= 0) return;

    int skipScr = _vga.Width() - copyX;
    int skipSour = map.width - copyX;

    uint8_t* img = mapData + (syu * map.width) + sxl;
    if (_vga.BPP() == 16) {
        uint16_t* scr = &_vga.lineBuf16[_vga.backBufLine + y + syu][x + sxl];
  
        while(copyY-- > 0){
            for (int i = 0; i < copyX; i++) *scr++ = (uint16_t)*img++;
            scr += skipScr;
            img += skipSour;
        }  
    } else {
        uint8_t* scr = &_vga.lineBuf8[_vga.backBufLine + y + syu][x + sxl];
  
        while(copyY-- > 0){
            for (int i = 0; i < copyX; i++) *scr++ = *img++;
            scr += skipScr;
            img += skipSour;
        }  
    }
}

bool Tiles::loadTiles(const uint8_t *data, int num){
    if (!data || num < 0) return false;

    int bpp = *data++;
    if (bpp != 8 && bpp != 16) return false;
    
    int frames = *data++;
    if (num > frames) return false;
    
    return true;
}