#pragma once;

#include <stdint.h>

#define LUT_SIZE 360
#define MAX_LINES 2048
#define ALIGNMENT_SRAM 4
#define ALIGNMENT_PSRAM 64
#define PTR_OFFSET(ptr, offset)         ((void*)((uint8_t*)(ptr) + (offset)))
#define PTR_OFFSET_T(ptr, offset, type) ((type*)((uint8_t*)(ptr) + (offset)))

#define CAPS_PSRAM (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define CAPS_BOUNCE (MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT)

struct Pins{ 
	int r[5], g[6], b[5];
	int h, v; 
    int pClk;
};

#define ESP32_S3 0
// ┌─ ESP32 S3 pins ────────────────────────────────────────────────────────────────┐
// │ - Camera                                                                       │
// │ ~ SD card                                                                      │
// │ * PSRam                                                                        │
// ├────────────────────────────────────────────────────────────────────────────────┤
// │           _   _   _   _  _          _    _   _   _   _  _   _   _  _           │
// │ 5V,  14, 13, 12, 11, 10, 9, 46, 3,  8,  18, 17, 16, 15, 7,  6,  5, 4, EN, 3V3  │
// │ GND, 19, 20, 21, 47, 48, 45, 0, 35, 36, 37, 38, 39, 40, 41, 42, 2, 1, RX, TX   │
// │                                  *   *   *   ~   ~   ~                         │
// └────────────────────────────────────┬───────────────────────────────────────────┤
        inline Pins defPins_S3 = {  //  │                                           │
            15, 7, 6, 5, 4,         //  │   R0..R4 (5 bit)                          │
            14, 9, 8, 18, 17, 16,   //  │   G0..G5 (6 bit)                          │
            21, 13, 12, 11, 10,     //  │   B0..B5 (5 bit)                          │
            1, 2,                   //  │   HSYNC, VSYNC                            │
            20                      //  │   PClkPIN                                 │
        };                          //  │                                           │
// ┌────────────────────────────────────┴───────────────────────────────────────────┤
// │           _   _   _   _  _          _    _   _   _   _  _   _   _  _           │
// │ 5V,                         46, 3,                                    EN, 3V3  │
// │ GND, 19,         47, 48, 45, 0, 35, 36, 37, 38, 39, 40, 41, 42,       RX, TX   │
// │                                  *   *   *   ~   ~   ~                         │                                 
// └────────────────────────────────────────────────────────────────────────────────┘
    
#define ESP32_P4 1
// ┌─ ESP32 P4 pins ───────────────────────────────────────────────────────────────┐
// ├───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┤
// │ 5V│ 5V│GND│TXD│RXD│ 22│GND│  5│  4│GND│  1│ 36│ 32│ 25│GND│ 54│GND│ 46│ 27│ 45│
// │3V3│SDA│SCL│ 23│GND│ 21│ 20│  6│3V3│  3│  2│  0│GND│ 24│ 33│ 26│ 48│ 53│ 47│GND│
// └───┴───┴───┴───┴───┴───┴───┴───┴───┼───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┤
        inline Pins defPins_P4 = {  // │                                           │
            6, 20, 21, 22, 23,      // │    R0..R4 (5 bit)                         │
            25, 32, 2, 3, 4, 5,     // │    G0..G5 (6 bit)                         │
            27, 53, 26, 33, 24,     // │    B0..B4 (5 bit)                         │
            0, 1,                   // │    HSYNC, VSYNC                           │
            54                      // │    PClkPIN                                │
        };                          // │                                           │            
// ┌───┬───┬───┬───┬───┬───┬───┬───┬───┼───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┤
// │ 5V│ 5V│GND│TXD│RXD│   │GND│   │   │GND│   │ 36│   │   │GND│ 54│GND│ 46│   │ 45│
// │3V3│SDA│SCL│   │GND│   │   │   │3V3│   │   │   │GND│   │   │   │ 48│   │ 47│GND│
// └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘

struct Mode {
    int pclk_hz;
    int hRes, hFront, hSync, hBack, hPol;
    int vRes, vFront, vSync, vBack, vPol;
};

inline Mode MODE640x480_60Hz = {25'000'000, 640, 16, 96, 48, 1, 480, 10, 2, 33, 1};
inline Mode MODE800x600_60Hz = {40'000'000, 800, 40, 128, 88, 1, 600, 1, 4, 23, 1};
inline Mode MODE1024x768_60Hz = {65'000'000, 1024, 24, 136, 160, 0, 768, 3, 6, 29, 0};

struct Screen{
    uint8_t scale;
    uint8_t bpp;
    uint16_t maxCol;
	int width, height;
	int xx, yy;
	int cx, cy;
};

struct Viewport{
    int x1, y1;
    int x2, y2;
};

struct RGBPanel{
    int bufSize;
    int lines;
    int lastPos;
};

struct FrameBufferConfig {
    bool dBuff;
    int shift;    
    uint8_t pixelSize;
    uint16_t lineSize;
    uint32_t frameSize;
    uint32_t frameFullSize;
    uint32_t fullSize;

    //RGBPanel panel;
};

struct BounceConfig{
    int lines, tik;  
    uint16_t copyBytes;
    uint16_t copyBytes2X;
    int lastPos; 
};

static uint8_t alpha_mul[256];   // alpha_mul[a] = a
static uint8_t alpha_imul[256];  // alpha_imul[a] = 255 - a

union RGBColor{
    uint16_t col16[32][64][32];
    uint8_t col8[3][3][2];
};

enum TextAlign {
    LEFT,
    CENTER,
    RIGHT
};

//#include "esp_cache.h"
//esp_cache_msync(buf16, _fbc.frameFullSize, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
//uint32_t caps = (_fbc.psRam ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL) | 
//                MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
//    uint32_t caps = (_fbc.psRam ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL) | 
//                    MALLOC_CAP_DMA | MALLOC_CAP_32BIT;
//    Serial.printf("\nMode: %dx%dx%d\n", _scr.width, _scr.height, _scr.bpp);