#include <Arduino.h>
#include "VGA_esp32s3p4.h"
#include <esp_LCD_panel_ops.h>
#include <esp_heap_caps.h>
#include "esp_cache.h"

VGA_esp32s3p4::VGA_esp32s3p4() {
    #if defined(CONFIG_IDF_TARGET_ESP32P4) || defined(ARDUINO_ESP32P4_DEV) || defined(ESP32P4)   // ESP32-P4
        _pins = defPins_P4;
        allingBuff = 64;
    #else
        _pins = defPins_S3;
        allingBuff = 32;
    #endif

    //Init LUT
    for (int i = 0; i < LUT_SIZE; ++i) {
        sinLUT[i] = (int16_t)(1000.0 * sin(i * DEG_TO_RAD));
        cosLUT[i] = (int16_t)(1000.0 * cos(i * DEG_TO_RAD));
    }   
    
    //FPS
    frameCount = 0;
    fpsStartTime = millis(); 
    cLine = 0;
}

VGA_esp32s3p4::~VGA_esp32s3p4() {

}

void VGA_esp32s3p4::correctLine(int x){
    cLine = (_scr.bpp == 16 ? x * 2 : x);
}

bool VGA_esp32s3p4::init(Mode &m, int bpp, int scale, int dBuff) {
    _mode = m;

    auto& s = _scr;
    s.scale = std::clamp(scale, 0, 2);
    s.bpp = ((bpp == 16) ? 16 : 8);
    s.maxCol = ((bpp == 16) ? 65536 : 256);
    s.width = m.hRes >> scale;
    s.height = m.vRes >> scale;
    s.xx = s.width - 1;
    s.yy = s.height - 1;
    s.cx = s.width / 2;
    s.cy = s.height / 2;
    _fbc.dBuff = dBuff;

    setViewport(0, 0, s.xx, s.yy);
    if (!allocateMemory()) return false;

    setRGBPanel();
    esp_err_t err = esp_lcd_new_rgb_panel(&panel_config, &panel_handle);
    if (err != ESP_OK) {
        Serial.printf("esp_lcd_new_rgb_panel error: %d\n", err);
        return false;
    }

    regSemaphore();
    regCallBack();   

    err = esp_lcd_panel_reset(panel_handle);
    if (err != ESP_OK) {
        Serial.printf("ERROR: Failed to reset panel: %s\n", esp_err_to_name(err));
        return false;
    }

    err = esp_lcd_panel_init(panel_handle);
    if (err != ESP_OK) {
        Serial.printf("ERROR: Failed to init panel: %s\n", esp_err_to_name(err));
        return false;
    }

    Serial.println("Panel initialized...Ok");
    return true;
}

bool VGA_esp32s3p4::initBG() {
    if (_fbc.frameFullSize <= 0) return false;
    if (bg) free(bg); bg = nullptr;
 
    bg = (uint8_t*) heap_caps_aligned_alloc(allingBuff, _fbc.frameFullSize, CAPS_PSRAM);
    if (!bg) {
        Serial.println("ERROR: cannot allocate background buffer");
        return false;
    }
    memset(bg, 0, _fbc.frameFullSize);

    return true;
}

void VGA_esp32s3p4::copyScrToBG(){
    if (!bg) initBG();
    memcpy(bg, PTR_OFFSET_T(buf, backBuf, uint8_t), _fbc.frameFullSize);
}

void VGA_esp32s3p4::copyBGToScr(){
    if (!bg) return;
    memcpy(PTR_OFFSET_T(buf, backBuf, uint8_t), bg,_fbc.frameFullSize);
}

bool VGA_esp32s3p4::allocateMemory() {
    if ((_scr.bpp != 8 && _scr.bpp != 16) || (_scr.width <= 0) || (_scr.height <= 0)) return false;

    _fbc.shift = (_scr.bpp == 16) ? 1 : 0;
    _fbc.pixelSize = ((_scr.bpp == 16) ? 2 : 1);
    _fbc.lineSize = _scr.width * _fbc.pixelSize;
    _fbc.frameSize = _scr.width * _scr.height;
    _fbc.frameFullSize = _scr.height * _fbc.lineSize;
    _fbc.fullSize = _fbc.frameFullSize * (_fbc.dBuff ? 2 : 1);
    frontBuf = 0;      backBuf        = (_fbc.dBuff ? _fbc.frameFullSize    : 0);
    frontBufLine = 0;  backBufLine    = (_fbc.dBuff ? _scr.height           : 0);

    buf = (uint8_t*) heap_caps_aligned_alloc(allingBuff, _fbc.fullSize, CAPS_PSRAM);
    if (!buf) {
        Serial.println("ERROR: cannot allocate framebuffer");
        return false;
    } else {
        memset(buf, 0, _fbc.fullSize);
        esp_cache_msync(buf, _fbc.fullSize, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

        int lines = _scr.height << _fbc.dBuff;
        if (lines > MAX_LINES) {
            Serial.println("ERROR: too many lines for lineBuf");
            heap_caps_free(buf); buf = nullptr;
            return false;
        } 

        if (_scr.bpp == 16){
            uint16_t *line = (uint16_t*) buf;
            for (int i = 0; i < lines; i++) {
                lineBuf16[i] = line;
                line += _scr.width;
            }  
        } else {
            uint8_t *line = buf;
            for (int i = 0; i < lines; i++) {
                lineBuf8[i] = line;
                line += _scr.width;
            }  
        }          
    }
    
    tmpBuf = (uint8_t*) heap_caps_aligned_alloc(allingBuff, _fbc.lineSize, CAPS_PSRAM); 
    if (!tmpBuf) {
        heap_caps_free(buf); buf = nullptr;
        Serial.println("ERROR: cannot allocate line buffer");

        return false;
    } else {
        memset(tmpBuf, 0, _fbc.lineSize);
    }    

    Serial.println("AllocateMemory...ok");
    return true;
}

void VGA_esp32s3p4::setRGBPanel(){
    auto& m = _mode;
    memset(&panel_config, 0, sizeof(panel_config));

    //Timing
    panel_config.clk_src = LCD_CLK_SRC_DEFAULT;
    panel_config.timings.pclk_hz = m.pclk_hz;
    panel_config.timings.h_res   = m.hRes;
    panel_config.timings.v_res   = m.vRes;

    panel_config.timings.hsync_pulse_width = m.hSync;
    panel_config.timings.hsync_back_porch  = m.hBack;
    panel_config.timings.hsync_front_porch = m.hFront;
    panel_config.timings.flags.hsync_idle_low = (m.hPol == 1) ^ 1;

    panel_config.timings.vsync_pulse_width = m.vSync;
    panel_config.timings.vsync_back_porch  = m.vBack;
    panel_config.timings.vsync_front_porch = m.vFront;
    panel_config.timings.flags.vsync_idle_low = (m.vPol == 1) ^ 1;
    
    panel_config.timings.flags.de_idle_high = true;
    panel_config.timings.flags.pclk_active_neg = true;
    panel_config.timings.flags.pclk_idle_high = true;

    //Panel config
    panel_config.data_width = (_scr.bpp == 16 ? 16 : 8);        
    panel_config.bits_per_pixel = (_scr.bpp == 16 ? 16 : 8);   
    panel_config.num_fbs = 0;
    panel_config.bounce_buffer_size_px = optimal_bounce_buffer_px();
    panel_config.sram_trans_align = ALIGNMENT_SRAM;
    panel_config.psram_trans_align = ALIGNMENT_PSRAM;
    //panel_config.dma_burst_size = 64;

    //Pins config
    if (_scr.bpp == 16){
        // B0..B4
        for (int i = 0; i < 5; i++)
            panel_config.data_gpio_nums[i] = _pins.b[i];

        // G0..G5
        for (int i = 0; i < 6; i++)
            panel_config.data_gpio_nums[5 + i] = _pins.g[i];

        // R0..R4
        for (int i = 0; i < 5; i++)
            panel_config.data_gpio_nums[11 + i] = _pins.r[i];
    } else {
        panel_config.data_gpio_nums[0] = _pins.b[3];
        panel_config.data_gpio_nums[1] = _pins.b[4];
        panel_config.data_gpio_nums[2] = _pins.g[3];
        panel_config.data_gpio_nums[3] = _pins.g[4];
        panel_config.data_gpio_nums[4] = _pins.g[5];
        panel_config.data_gpio_nums[5] = _pins.r[2];
        panel_config.data_gpio_nums[6] = _pins.r[3];
        panel_config.data_gpio_nums[7] = _pins.r[4];
    }
    panel_config.hsync_gpio_num = _pins.h;
    panel_config.vsync_gpio_num = _pins.v;
    panel_config.de_gpio_num = -1;
    panel_config.pclk_gpio_num = _pins.pClk;
    panel_config.disp_gpio_num = -1;

    //Flags
    panel_config.flags.disp_active_low = true;
    panel_config.flags.refresh_on_demand = false;
    panel_config.flags.fb_in_psram = false;
    panel_config.flags.double_fb = false;
    panel_config.flags.no_fb = true;
    panel_config.flags.bb_invalidate_cache = false;
    
    Serial.println("RGB Panel set ok");
}

int VGA_esp32s3p4::optimal_bounce_buffer_px(){
    int res = 0;
    if (_mode.hRes == 640 && _mode.vRes == 480) res = ((_scr.bpp == 16) ? 15360 : 30720);
    
    int lines = res / _mode.hRes;
    _bc.lines = lines >> _scr.scale;
    _bc.tik = _mode.hRes >> 4;
    _bc.copyBytes = _mode.hRes * _fbc.pixelSize;
    _bc.copyBytes2X = _bc.copyBytes << 1;
    int total_px = _mode.hRes * _mode.vRes;  // пиксели панели
    _bc.lastPos = total_px - res;            // res = bounce_buffer_size_px

    Serial.printf("bounce_buffer_px: %d, lines: %d, copyBytes: %d\n", res, _bc.lines, _bc.copyBytes);
    return res;

/*
    int h   = _mode.hRes;
    int bpp = _scr.bpp;

    if (bpp == 8)  return h * 32;   // 32 строки
    if (bpp == 16) return h * 8;    // 8 строк

    return h * 16;
*/    
}

void VGA_esp32s3p4::regCallBack(){
    esp_lcd_rgb_panel_event_callbacks_t cb = {
        .on_color_trans_done = nullptr,
        .on_vsync            = on_vsync,
        .on_bounce_empty     = on_bounce_empty,                
        .on_frame_buf_complete = nullptr
    };

    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cb, this)); 
    Serial.println("Registered callBacks...Ok"); 
}

void VGA_esp32s3p4::regSemaphore(){
    if (_fbc.dBuff){
        sem_vsync_end = xSemaphoreCreateBinary();
        sem_gui_ready = xSemaphoreCreateBinary();
        assert(sem_vsync_end && sem_gui_ready);
    }

    Serial.println("Registered semaphores...Ok");
}

bool IRAM_ATTR VGA_esp32s3p4::on_color_trans_done(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx){

    return false;
}

bool IRAM_ATTR VGA_esp32s3p4::on_vsync(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx){
    VGA_esp32s3p4* vga = (VGA_esp32s3p4*) user_ctx;

    vga->timer++;

    return false;
}

bool IRAM_ATTR VGA_esp32s3p4::on_bounce_empty(
        esp_lcd_panel_handle_t panel,
        void *bounce_buf,
        int pos_px,
        int len_bytes,
        void *user_ctx)
{
    int tik, lines;
    VGA_esp32s3p4* vga = (VGA_esp32s3p4*) user_ctx;

    len_bytes -= vga->cLine;
    if (vga->_scr.bpp == 16){
        uint16_t *dest = (uint16_t*)(bounce_buf + vga->cLine);
        uint16_t* sour = PTR_OFFSET_T(vga->buf, vga->frontBuf, uint16_t);

        if (vga->_scr.scale == 0){            
            memcpy(dest, sour + pos_px, len_bytes);

        } else if (vga->_scr.scale == 1){
            sour += pos_px >> 2;
            int lines = vga->_bc.lines;
            uint16_t *savePos = nullptr; 

            while (lines-- > 0){
                savePos = dest;
                for (int i = 0; i < vga->_bc.tik; i++){
                    *(dest++) = *sour; *(dest++) = *(sour++); *(dest++) = *sour; *(dest++) = *(sour++); 
                    *(dest++) = *sour; *(dest++) = *(sour++); *(dest++) = *sour; *(dest++) = *(sour++); 
                    *(dest++) = *sour; *(dest++) = *(sour++); *(dest++) = *sour; *(dest++) = *(sour++); 
                    *(dest++) = *sour; *(dest++) = *(sour++); *(dest++) = *sour; *(dest++) = *(sour++); 
                }
                memcpy(dest, savePos, vga->_bc.copyBytes); dest += vga->_mode.hRes;
            } 
        } else {
            sour += pos_px >> 4;
            int lines = vga->_bc.lines;
            uint16_t *savePos = nullptr;         

            while (lines-- > 0){
                savePos = dest;
                for (int i = 0; i < vga->_bc.tik; i++){
                    *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *(sour++);  
                    *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *(sour++);  
                    *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *(sour++);  
                    *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *(sour++); 
                }
                memcpy(dest, savePos, vga->_bc.copyBytes); dest += vga->_mode.hRes;
                memcpy(dest, savePos, vga->_bc.copyBytes2X); dest += vga->_bc.copyBytes;
            }  
        }
    } else {
        uint8_t *dest = (uint8_t*)(bounce_buf + vga->cLine);
        uint8_t *sour = PTR_OFFSET_T(vga->buf, vga->frontBuf, uint8_t);

        if (vga->_scr.scale == 0){            
            memcpy(dest, sour + pos_px, len_bytes);

        } else if (vga->_scr.scale == 1){
            sour += pos_px >> 2;
            int lines = vga->_bc.lines;
            uint8_t *savePos = nullptr; 
            
            while (lines-- > 0){
                savePos = dest;
                for (int i = 0; i < vga->_bc.tik; i++){
                    *(dest++) = *sour; *(dest++) = *(sour++); *(dest++) = *sour; *(dest++) = *(sour++); 
                    *(dest++) = *sour; *(dest++) = *(sour++); *(dest++) = *sour; *(dest++) = *(sour++); 
                    *(dest++) = *sour; *(dest++) = *(sour++); *(dest++) = *sour; *(dest++) = *(sour++); 
                    *(dest++) = *sour; *(dest++) = *(sour++); *(dest++) = *sour; *(dest++) = *(sour++); 
                }
                memcpy(dest, savePos, vga->_bc.copyBytes); dest += vga->_mode.hRes;
            }            
        } else {
            sour += pos_px >> 4;
            int lines = vga->_bc.lines;
            uint8_t *savePos = nullptr;         

            while (lines-- > 0){
                savePos = dest;
                for (int i = 0; i < vga->_bc.tik; i++){
                    *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *(sour++);  
                    *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *(sour++);  
                    *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *(sour++);  
                    *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *sour; *(dest++) = *(sour++); 
                }
                memcpy(dest, savePos, vga->_bc.copyBytes); dest += vga->_mode.hRes;
                memcpy(dest, savePos, vga->_bc.copyBytes2X); dest += vga->_bc.copyBytes2X;
            }             
        }
    }

    BaseType_t hp_task_woken = pdFALSE;
    if (pos_px >= vga->_bc.lastPos && vga->_fbc.dBuff) {
        if (xSemaphoreTakeFromISR(vga->sem_gui_ready, &hp_task_woken) == pdTRUE) {
            std::swap(vga->frontBuf, vga->backBuf);
            std::swap(vga->frontBufLine, vga->backBufLine);
            xSemaphoreGiveFromISR(vga->sem_vsync_end, &hp_task_woken);
        }
    }   
      
    return (hp_task_woken == pdTRUE);
}

bool IRAM_ATTR VGA_esp32s3p4::on_frame_buf_complete(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx){

    return false;
}

//===============================================================================
void VGA_esp32s3p4::swap() {
    if (_fbc.dBuff){
        xSemaphoreGive(sem_gui_ready);
        xSemaphoreTake(sem_vsync_end, portMAX_DELAY);
    }
}

void VGA_esp32s3p4::setViewport(int x1, int y1, int x2, int y2){
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    _vp.x1 = max(0, x1);
    _vp.y1 = max(0, y1);
    _vp.x2 = min(x2, _scr.xx);
    _vp.y2 = min(y2, _scr.yy);
}

void VGA_esp32s3p4::setPins(Pins p){
    _pins = p;
}

void VGA_esp32s3p4::setPins(
    int r0, int r1, int r2, int r3, int r4,
    int g0, int g1, int g2, int g3, int g4, int g5,
    int b0, int b1, int b2, int b3, int b4,
    int hsync, int vsync,
    int pClkPin)
{
    // Заполняем структуру
    _pins.r[0] = r0; _pins.r[1] = r1; _pins.r[2] = r2; _pins.r[3] = r3; _pins.r[4] = r4;
    _pins.g[0] = g0; _pins.g[1] = g1; _pins.g[2] = g2; _pins.g[3] = g3; _pins.g[4] = g4; _pins.g[5] = g5;
    _pins.b[0] = b0; _pins.b[1] = b1; _pins.b[2] = b2; _pins.b[3] = b3; _pins.b[4] = b4;
    _pins.h = hsync;
    _pins.v = vsync;
    _pins.pClk = pClkPin;
}

void VGA_esp32s3p4::LUT(int &x, int &y, int xx, int yy, int len, int angle) {
    angle %= LUT_SIZE;
    if (angle < 0) angle += 360;
    x = xx + (len * cosLUT[angle]) / 1000;
    y = yy + (len * sinLUT[angle]) / 1000;
}

int VGA_esp32s3p4::xLUT(int x, int len, int angle) {
    angle %= LUT_SIZE;
    if (angle < 0) angle += 360;
    return x + (len * cosLUT[angle]) / 1000;
}

int VGA_esp32s3p4::yLUT(int y, int len, int angle) {
    angle %= LUT_SIZE;
    if (angle < 0) angle += 360;
    return y + (len * sinLUT[angle]) / 1000;
}

void VGA_esp32s3p4::updateFPS(){
    uint64_t now = millis();
    frameCount++;

    if (now - fpsStartTime >= 1000) {
        fps = frameCount * 1000.0f / (now - fpsStartTime);
        frameCount = 0;
        fpsStartTime = now;
    }
}

void VGA_esp32s3p4::testRGBPanel() {
    if (!buf) return;

    int cx = _scr.cx - 128;
    int cy = _scr.cy - 45;

    // clamp 256x90
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx + 256 > _scr.width)  cx = _scr.width  - 256;
    if (cy + 90  > _scr.height) cy = _scr.height - 90;

    const int midX = _scr.width / 2;
    const int midY = _scr.height / 2;

    if (_scr.bpp == 16) {
        // ---- фон (градиент) ----
        for (int y = 0; y < _scr.height; y++) {
            uint8_t gy = (uint32_t)y * 255 / (_scr.height - 1);
            uint16_t gg = (uint16_t)(gy >> 2);      // 0..63

            uint16_t* row = lineBuf16[backBufLine + y];
            for (int x = 0; x < _scr.width; x++) {
                uint8_t rx = (uint32_t)x * 255 / (_scr.width - 1);
                uint16_t rr = (uint16_t)(rx >> 3);          // 0..31
                uint16_t bb = (uint16_t)((255 - rx) >> 3);  // 0..31
                row[x] = (rr << 11) | (gg << 5) | bb;
            }
        }

        // ---- тестовые полоски 256x90 ----
        for (int y = 0; y < 30; y++) {
            for (int x = 0; x < 256; x++) {
                lineBuf16[backBufLine + cy + y][cx + x]      = (uint16_t)(x >> 3) << 11; // R
                lineBuf16[backBufLine + cy + y + 30][cx + x] = (uint16_t)(x >> 2) << 5;  // G
                lineBuf16[backBufLine + cy + y + 60][cx + x] = (uint16_t)(x >> 3);       // B
            }
        }

        // ---- белая рамка + линии по центру ----
        const uint16_t WHITE = 0xFFFF;

        // рамка: верх/низ
        uint16_t* top = lineBuf16[backBufLine + 0];
        uint16_t* bot = lineBuf16[backBufLine + (_scr.height - 1)];
        for (int x = 0; x < _scr.width; x++) {
            top[x] = WHITE;
            bot[x] = WHITE;
        }
        // рамка: лево/право
        for (int y = 0; y < _scr.height; y++) {
            uint16_t* row = lineBuf16[backBufLine + y];
            row[0] = WHITE;
            row[_scr.width - 1] = WHITE;
        }

        // вертикальная линия по центру
        for (int y = 0; y < _scr.height; y++) {
            lineBuf16[backBufLine + y][midX] = WHITE;
        }
        // горизонтальная линия по центру
        uint16_t* mid = lineBuf16[backBufLine + midY];
        for (int x = 0; x < _scr.width; x++) {
            mid[x] = WHITE;
        }

    } else {
        // ---- фон (градиент) ----
        for (int y = 0; y < _scr.height; y++) {
            uint8_t gy = (uint32_t)y * 255 / (_scr.height - 1);
            uint8_t gg = gy >> 5;   // 0..7

            uint8_t* row = lineBuf8[backBufLine + y];
            for (int x = 0; x < _scr.width; x++) {
                uint8_t rx = (uint32_t)x * 255 / (_scr.width - 1);
                uint8_t rr = rx >> 5;          // 0..7
                uint8_t bb = (255 - rx) >> 6;  // 0..3
                row[x] = (uint8_t)((rr << 5) | (gg << 2) | bb);
            }
        }

        // ---- тестовые полоски 256x90 ----
        for (int y = 0; y < 30; y++) {
            for (int x = 0; x < 256; x++) {
                lineBuf8[backBufLine + cy + y][cx + x]      = (uint8_t)(x >> 5) << 5; // R
                lineBuf8[backBufLine + cy + y + 30][cx + x] = (uint8_t)(x >> 5) << 2; // G
                lineBuf8[backBufLine + cy + y + 60][cx + x] = (uint8_t)(x >> 6);      // B
            }
        }

        // ---- белая рамка + линии по центру ----
        const uint8_t WHITE = 0xFF; // RGB332: R=7,G=7,B=3

        // рамка: верх/низ
        uint8_t* top = lineBuf8[backBufLine + 0];
        uint8_t* bot = lineBuf8[backBufLine + (_scr.height - 1)];
        for (int x = 0; x < _scr.width; x++) {
            top[x] = WHITE;
            bot[x] = WHITE;
        }
        // рамка: лево/право
        for (int y = 0; y < _scr.height; y++) {
            uint8_t* row = lineBuf8[backBufLine + y];
            row[0] = WHITE;
            row[_scr.width - 1] = WHITE;
        }

        // вертикальная линия по центру
        for (int y = 0; y < _scr.height; y++) {
            lineBuf8[backBufLine + y][midX] = WHITE;
        }
        // горизонтальная линия по центру
        uint8_t* mid = lineBuf8[backBufLine + midY];
        for (int x = 0; x < _scr.width; x++) {
            mid[x] = WHITE;
        }
    }
}

/*
void VGA_esp32s3p4::testRGBPanel(){
    if (!buf) return;

    int cx = _scr.cx - (256 / 2);
    int cy = _scr.cy - (90 / 2); 

    if (_scr.bpp == 16){
        //uint16_t *
        for (int y = 0; y < _scr.height; y++){
            uint16_t gg = (y > 0 ? (y * 64) / _scr.height : 0);
            for (int x = 0; x < _scr.width; x++){
                uint16_t rr = (x > 0 ? (x * 32) / (_scr.width) : 0);
                uint16_t bb = (x > 0 ? ((_scr.width - x) * 32) / _scr.width : 0);
                lineBuf16[backBufLine + y][x] = (rr << 11) | (gg << 5) | bb;
            }
        }

        for (int y = 0; y < 30; y++){
            for (int x = 0; x < 256; x++){
                lineBuf16[backBufLine + cy + y][cx + x] = (uint16_t)(x >> 3) << 11;
                lineBuf16[backBufLine + cy + y + 30][cx + x] = (uint16_t)(x >> 2) << 5;
                lineBuf16[backBufLine + cy + y + 60][cx + x] = (uint16_t)(x >> 3);
            }
        }        
    } else {
        for (int y = 0; y < _scr.height; y++){
            uint8_t gg = (y > 0 ? (uint8_t)(y * 8 / _scr.height) : 0);
            for (int x = 0; x < _scr.width; x++){
                uint8_t rr = (x > 0 ? (uint8_t)(x * 8 / _scr.width) : 0);
                uint8_t bb = (x > 0 ? (uint8_t)((_scr.width - x) * 4 / _scr.width) : 0);
                lineBuf8[backBufLine + y][x] = (rr << 5) | (gg << 2) | bb;
            }
        }

        for (int y = 0; y < 30; y++){
            for (int x = 0; x < 256; x++){
                lineBuf8[backBufLine + cy + y][cx + x] = (uint8_t)(x >> 5) << 5;
                lineBuf8[backBufLine + cy + y + 30][cx + x] = (uint8_t)(x >> 5) << 2;
                lineBuf8[backBufLine + cy + y + 60][cx + x] = (uint8_t)(x >> 6);
            }
        }
    }
}
void VGA_esp32s3p4::scrollY(int sy){
    if (sy == 0 || sy % _scr.height == 0) return;

    // ограничиваем сдвиг буфером на 10 строк
    sy = std::clamp(sy, -10, 10);

    uint8_t* scr = (_scr.bpp == 16 ? PTR_OFFSET_T(buf16, backBuf, uint8_t) :
                                     PTR_OFFSET_T(buf8, backBuf, uint8_t));

    int sizeY = _scr.height - abs(sy);
    int copyBytes = _fbc.lineSize * abs(sy);

    if (sy > 0) { // вверх
        memcpy(tmpBuf, scr, copyBytes);
        while (sizeY-- > 0){
            memcpy(scr, scr + copyBytes, _fbc.lineSize);
            scr += _fbc.lineSize;
        }
        memcpy(scr, tmpBuf, copyBytes);
    } else { // вниз
        uint8_t* savePos = scr;
        scr += _fbc.frameFullSize - copyBytes - _fbc.lineSize;
        memcpy(tmpBuf, scr, copyBytes);

        while (sizeY-- > 0){
            memcpy(scr + copyBytes, scr, _fbc.lineSize);
            scr -= _fbc.lineSize;
        }
            
        memcpy(savePos, tmpBuf, copyBytes);
    }
}


void VGA_esp32s3p4::scrollX(int sx){
    sx %= _scr.width;
    if (sx == 0) return;

    uint8_t* scr = (_scr.bpp == 16 ? PTR_OFFSET_T(buf16, backBuff, uint8_t) :
                                     PTR_OFFSET_T(buf8, backBuff, uint8_t));

    int shift = abs(sx);
    int sizeY = _scr.height;
    int copyBytes = shift << (_scr.bpp == 16 ? 1 : 0);
    int copyBlock = _fbc.lineSize - copyBytes;
    int skip = _fbc.lineSize;

    if (sx > 0){// влево  
        while (sizeY-- > 0){                
            memcpy(tmpBuf, scr + copyBytes, copyBlock);
            memcpy(scr + copyBlock, scr, copyBytes);
            memcpy(scr, tmpBuf, copyBlock);
            scr += skip;
        }    
    } else {// вправо            
        
        int copyRight = _scr.width - shift;

        while (sizeY-- > 0) {
            memcpy(tmpBuf, scr, copyBlock);
            memcpy(scr, scr + copyBlock, copyBytes);
            memcpy(scr + copyBytes, tmpBuf, copyBlock);
            scr += skip;  
        }
    }
}

void VGA_esp32s3p4::scrollXY(int sx, int sy){
    sx %= _scr.width; 
    sy = std::clamp(sy, -10, 10);
    if (sx == 0 && sy == 0) return;

    uint8_t* scrX = (_scr.bpp == 16 ? PTR_OFFSET_T(buf16, backBuff, uint8_t) :
                                     PTR_OFFSET_T(buf8, backBuff, uint8_t));
    uint8_t* scrY = scrX;                                    

    if (sx != 0){
        int shift = abs(sx);
        int sizeY = _scr.height;        
        int copyBytes = shift << (_scr.bpp == 16 ? 1 : 0);
        int copyBlock = _fbc.lineSize - copyBytes;
        int skip = _fbc.lineSize;

        if (sx > 0){
            while (sizeY-- > 0){
                memcpy(tmpBuf, scrX + copyBytes, copyBlock);
                memcpy(scrX + copyBlock, scrX, copyBytes);
                memcpy(scrX, tmpBuf, copyBlock);
                scrX += skip;
            }
        } else {
            while (sizeY-- > 0){
                memcpy(tmpBuf, scrX, copyBlock);
                memcpy(scrX, scrX + copyBlock, copyBytes);
                memcpy(scrX + copyBytes, tmpBuf, copyBlock);
                scrX += skip;                
            }
        }
    }

    if (sy != 0){
        int sizeY = _scr.height - abs(sy);
        int lineSize = _fbc.lineSize;
        int copyBytes = lineSize * abs(sy);
        
        if (sy > 0) { // вверх
            memcpy(tmpBuf, scrY, copyBytes);
            while (sizeY-- > 0){
                memcpy(scrY, scrY + copyBytes, lineSize);
                scrY += lineSize;
            }
            memcpy(scrY, tmpBuf, copyBytes);
        } else { // вниз
            uint8_t* savePos = scrY;
            scrY += _fbc.frameFullSize - copyBytes - lineSize;
            memcpy(tmpBuf, scrY, copyBytes);

            while (sizeY-- > 0){
                memcpy(scrY + copyBytes, scrY, lineSize);
                scrY -= lineSize;
            }
            
            memcpy(savePos, tmpBuf, copyBytes);
        }
    }
}

*/


