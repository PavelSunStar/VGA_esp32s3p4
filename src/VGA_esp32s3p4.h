#pragma once;

#include "VGA_types.h"
#include "esp_lcd_panel_rgb.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <algorithm>
#include <cstring>

class VGA_esp32s3p4{
    public:
        uint8_t *bg = nullptr;
        uint8_t *buf = nullptr; 
        uint8_t *tmpBuf = nullptr;
        uint8_t *lineBuf8[MAX_LINES];
        uint16_t *lineBuf16[MAX_LINES];
        uint8_t *tmp[MAX_LINES];        
        int frontBuf, backBuf;
        int frontBufLine, backBufLine;

        VGA_esp32s3p4();
        ~VGA_esp32s3p4();

        //Screen
        int BPP()           {return _scr.bpp;};  
        int MaxCol()        {return _scr.maxCol;};
        int Shift()         {return _fbc.shift;};              
		int Width()         {return _scr.width;};
		int Height()        {return _scr.height;};
		int XX()            {return _scr.xx;};
		int YY()            {return _scr.yy;};
		int CX()            {return _scr.cx;};
		int CY()            {return _scr.cy;};
        int LineSize()      {return _fbc.lineSize;};
        int FrameFullSize() {return _fbc.frameFullSize;};        

        //Viewport
        int vX1(){return _vp.x1;};
        int vY1(){return _vp.y1;};
        int vX2(){return _vp.x2;};
        int vY2(){return _vp.y2;};

        //FPS 
        float FPS(){return fps;};

        //Timer
        uint32_t Timer(){return timer;};

        void setPins(Pins p);    
        void setPins(
            int r0, int r1, int r2, int r3, int r4,
            int g0, int g1, int g2, int g3, int g4, int g5,
            int b0, int b1, int b2, int b3, int b4,
            int hsync, int vsync,
            int pClkPin);

        void copyScrToBG();
        void copyBGToScr();

        void correctLine(int x = 0);
        bool init(Mode &m, int bpp = 8, int scale = 0, int dBuff = false);
        void setViewport(int x1, int y1, int x2, int y2);
        void testRGBPanel();
        void scrollY(int sy);
        void scrollX(int sx);
        void scrollXY(int sx, int sy);
        void swap();

        void updateFPS();
        void LUT(int &x, int &y, int xx, int yy, int len, int angle);
        int xLUT(int x, int len, int angle);
        int yLUT(int y, int len, int angle);        

    private:
        int16_t sinLUT[LUT_SIZE];
        int16_t cosLUT[LUT_SIZE];

        bool initBG();
        void setRGBPanel();
        bool allocateMemory();
        void regSemaphore();
        void regCallBack();
        int optimal_bounce_buffer_px();

        BounceConfig _bc;
        Mode _mode;
        Pins _pins;
        Screen _scr;
        Viewport _vp;
        FrameBufferConfig _fbc;
        uint32_t allingBuff;

        // FPS
        float fps = 0.0f;
        volatile uint32_t frameCount = 0;  // ISR
        uint64_t fpsStartTime = 0;         // μs
        volatile uint32_t timer = 0;       // vsync counter
        int cLine = 0;
                
        esp_lcd_rgb_panel_config_t panel_config;
        esp_lcd_panel_handle_t panel_handle = nullptr;
        SemaphoreHandle_t sem_vsync_end;
	    SemaphoreHandle_t sem_gui_ready;  
        
        static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx);
        static bool IRAM_ATTR on_vsync(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx);
        static bool IRAM_ATTR on_bounce_empty(esp_lcd_panel_handle_t panel, void *bounce_buf, int pos_px, int len_bytes, void *user_ctx);
        static bool IRAM_ATTR on_frame_buf_complete(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx);
};

/*
что значат +++ это при scale 0 1 2 работает ли режим или нет

Режим	hRes	bounce_px	было	комментарий
512×384×8	512	16384	16384	✔ совпадает, стабильно
640×350×8	640	20480	16000	🔥 улучшено (fix +--)
640×400×8	640	20480	25600	можно 20480 или 25600; 20480 быстрее
640×480×8	640	20480	30720	30720 тоже стабильно, можно оставить
720×400×8	720	23040	14400	14400 было мало; 23040 = идеально
768×576×8	768	24576	18432	18432 = 24 строки, норм, но 24576 лучше
800×600×8	800	25600	16000	16000 мало, 25600 = стабильно

1024×768×8	1024	32768	49152	49152 тоже норм, но 32768 быстрее и стабильно
MODE512x384x8 -> 16384
MODE640x350x8 -> 20480
MODE640x400x8 -> 20480  (или 25600 если хочешь)
MODE640x480x8 -> 20480  (30720 тоже можно, но больше нагрузки)
MODE720x400x8 -> 23040
MODE768x576x8 -> 24576
MODE800x600x8 -> 25600
MODE1024x768x8 -> 32768  (49152 тоже работает, но тяжелее)
--------------------------------------------------------------------------------------
Режим	hRes	bounce_px	было	комментарий
512×384×16	512	4096	16384	16384 избыточно, но ок
640×350×16	640	5120	16000	16000 слишком много → fix
640×400×16	640	5120	25600	огромный запас, но можно уменьшить
640×480×16	640	5120	15360	снова слишком много
720×400×16	720	5760	14400	14400 можно, но 5760 идеал
768×576×16	768	6144	18432	уменьшить для стабильности
800×600×16	800	6400	16000	16000 даёт "---" → 6400 фиксирует
1024×768×16	1024	8192	49152	49152 слишком тяжело → отсюда "---" → исправляем

MODE512x384x16 -> 4096
MODE640x350x16 -> 5120
MODE640x400x16 -> 5120
MODE640x480x16 -> 5120
MODE720x400x16 -> 5760
MODE768x576x16 -> 6144
MODE800x600x16 -> 6400
MODE1024x768x16 -> 8192
----------------------------------------------------------------------------------------
⭐ Итог: все "−−−", "+−−", "-++" — ИСПРАВЛЕНЫ
Ваши старые значения ломались потому что:
16-бит ISR → слишком много строк → не успевает заполнять
у 8-бит где "+--" → буфер слишком маленький (ISR слишком частый)
Теперь всё оптимально.

int VGA_esp32s3p4::optimal_bounce_buffer_px() {
    int h = _mode.hRes;
    int v = _mode.vRes;
    int bpp = _scr.bpp;

    // 8 BIT MODES
    if (bpp == 8) {
        return h * 32;  // универсально и стабильно
    }

    // 16 BIT MODES
    if (bpp == 16) {
        return h * 8;   // лучший баланс
    }

    return h * 16;  // fallback
}
*/