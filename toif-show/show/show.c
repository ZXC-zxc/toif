/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (c) SatoshiLabs
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include "uzlib/src/uzlib.h"
#include "fonts.h"

#define RGB16(R, G, B) ((R & 0xF8) << 8) | ((G & 0xFC) << 3) | ((B & 0xF8) >> 3)
#define COLOR_WHITE 0xFFFF
#define COLOR_BLACK 0x0000

#define COLOR_RED RGB16(0xFF, 0x00, 0x00)     // red
#define COLOR_GREEN RGB16(0x00, 0xAE, 0x0B)     // green
#define COLOR_BLUE RGB16(0x4A, 0x90, 0xE2)  // blue

#define TREZOR_MODEL   T
#ifndef   MIN
#define MIN(a, b)         (((a) < (b)) ? (a) : (b))
#endif

#ifndef   MAX
#define MAX(a, b)         (((a) > (b)) ? (a) : (b))
#endif

#define EMULATOR_BORDER 16
#define MAX_DISPLAY_RESX 320
#define MAX_DISPLAY_RESY 240
#define DISPLAY_RESX 320
#define DISPLAY_RESY 240
#define TREZOR_FONT_BPP 4

#define UZLIB_WINDOW_SIZE (1 << 10)

static int DISPLAY_BACKLIGHT = -1;
static int DISPLAY_ORIENTATION = -1;

#if TREZOR_MODEL == T

#ifdef TREZOR_EMULATOR_RASPI
#define WINDOW_WIDTH 480
#define WINDOW_HEIGHT 320
#define TOUCH_OFFSET_X 110
#define TOUCH_OFFSET_Y 40
#else
#define WINDOW_WIDTH 320
#define WINDOW_HEIGHT 240
#define TOUCH_OFFSET_X 80
#define TOUCH_OFFSET_Y 110
#endif

#elif TREZOR_MODEL == 1

#define WINDOW_WIDTH 200
#define WINDOW_HEIGHT 340
#define TOUCH_OFFSET_X 36
#define TOUCH_OFFSET_Y 92

#else
#error Unknown Trezor model
#endif

static SDL_Renderer* RENDERER;
static SDL_Surface* BUFFER;
static SDL_Texture* TEXTURE, * BACKGROUND;

static SDL_Surface* PREV_SAVED;

int sdl_display_res_x = DISPLAY_RESX, sdl_display_res_y = DISPLAY_RESY;
int sdl_touch_offset_x, sdl_touch_offset_y;

static struct {
    struct {
        uint16_t x, y;
    } start;
    struct {
        uint16_t x, y;
    } end;
    struct {
        uint16_t x, y;
    } pos;
} PIXELWINDOW;



void display_init_seq(void) {}

void display_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("%s\n", SDL_GetError());
    }
    atexit(SDL_Quit);

    char* window_title = "Trezor^emu";

    SDL_Window* win =
        SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED, MAX_DISPLAY_RESX, MAX_DISPLAY_RESY,
#ifdef TREZOR_EMULATOR_RASPI
            SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN
#else
            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
#endif
        );
    if (!win) {
        printf("%s\n", SDL_GetError());
    }
    RENDERER = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!RENDERER) {
        printf("%s\n", SDL_GetError());
        SDL_DestroyWindow(win);
    }
    SDL_SetRenderDrawColor(RENDERER, 0, 0, 0, 255);
    SDL_RenderClear(RENDERER);
    BUFFER = SDL_CreateRGBSurface(0, MAX_DISPLAY_RESX, MAX_DISPLAY_RESY, 16,
        0xF800, 0x07E0, 0x001F, 0x0000);
    TEXTURE = SDL_CreateTexture(RENDERER, SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING, DISPLAY_RESX,
        DISPLAY_RESY);
    SDL_SetTextureBlendMode(TEXTURE, SDL_BLENDMODE_BLEND);
#ifdef __APPLE__
    // macOS Mojave SDL black screen workaround
    SDL_PumpEvents();
    SDL_SetWindowSize(win, WINDOW_WIDTH, WINDOW_HEIGHT);
#endif
#ifdef TREZOR_EMULATOR_RASPI
#include "background_raspi.h"
    BACKGROUND = IMG_LoadTexture_RW(
        RENDERER, SDL_RWFromMem(background_raspi_jpg, background_raspi_jpg_len),
        0);
#else
#if TREZOR_MODEL == T
#include "background_T.h"
    BACKGROUND = IMG_LoadTexture_RW(
        RENDERER, SDL_RWFromMem(background_T_jpg, background_T_jpg_len), 0);
#elif TREZOR_MODEL == 1
#include "background_1.h"
    BACKGROUND = IMG_LoadTexture_RW(
        RENDERER, SDL_RWFromMem(background_1_jpg, background_1_jpg_len), 0);
#endif
#endif
    if (BACKGROUND) {
        SDL_SetTextureBlendMode(BACKGROUND, SDL_BLENDMODE_NONE);
        sdl_touch_offset_x = TOUCH_OFFSET_X;
        sdl_touch_offset_y = TOUCH_OFFSET_Y;
    }
    else {
        SDL_SetWindowSize(win, DISPLAY_RESX,DISPLAY_RESY);
        sdl_touch_offset_x = EMULATOR_BORDER;
        sdl_touch_offset_y = EMULATOR_BORDER;
    }
    DISPLAY_BACKLIGHT = 0;
#ifdef TREZOR_EMULATOR_RASPI
    DISPLAY_ORIENTATION = 270;
    SDL_ShowCursor(SDL_DISABLE);
#else
    DISPLAY_ORIENTATION = 0;
#endif
}

void PIXELDATA(uint16_t c) {
#if TREZOR_MODEL == 1
    // set to white if highest bits of all R, G, B values are set to 1
    // bin(10000 100000 10000) = hex(0x8410)
    // otherwise set to black
    c = (c & 0x8410) ? 0xFFFF : 0x0000;
#endif
    if (!RENDERER) {
        display_init();
    }
    if (PIXELWINDOW.pos.x <= PIXELWINDOW.end.x &&
        PIXELWINDOW.pos.y <= PIXELWINDOW.end.y) {
        ((uint16_t*)BUFFER->pixels)[PIXELWINDOW.pos.x + PIXELWINDOW.pos.y * BUFFER->pitch /sizeof(uint16_t)] = c;
    }
    PIXELWINDOW.pos.x++;
    if (PIXELWINDOW.pos.x > PIXELWINDOW.end.x) {
        PIXELWINDOW.pos.x = PIXELWINDOW.start.x;
        PIXELWINDOW.pos.y++;
    }
}

static void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1,
    uint16_t y1) {
    if (!RENDERER) {
        display_init();
    }
    PIXELWINDOW.start.x = x0;
    PIXELWINDOW.start.y = y0;
    PIXELWINDOW.end.x = x1;
    PIXELWINDOW.end.y = y1;
    PIXELWINDOW.pos.x = x0;
    PIXELWINDOW.pos.y = y0;
}

void display_refresh(void) {
    if (!RENDERER) {
        display_init();
    }
    if (BACKGROUND) {
        const SDL_Rect r = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
        SDL_RenderCopy(RENDERER, BACKGROUND, NULL, &r);
    }
    else {
        SDL_RenderClear(RENDERER);
    }
    SDL_UpdateTexture(TEXTURE, NULL, BUFFER->pixels, BUFFER->pitch);
#define BACKLIGHT_NORMAL 150
    SDL_SetTextureAlphaMod(TEXTURE,
        MIN(255, 255 * DISPLAY_BACKLIGHT / BACKLIGHT_NORMAL));
    if (BACKGROUND) {
        const SDL_Rect r = { TOUCH_OFFSET_X, TOUCH_OFFSET_Y, DISPLAY_RESX,
                            DISPLAY_RESY };
        SDL_RenderCopyEx(RENDERER, TEXTURE, NULL, &r, DISPLAY_ORIENTATION, NULL, 0);
    }
    else {
        const SDL_Rect r = { 0, 0, DISPLAY_RESX,
                            DISPLAY_RESY };
        SDL_RenderCopyEx(RENDERER, TEXTURE, NULL, &r, DISPLAY_ORIENTATION, NULL, 0);
    }
    SDL_RenderPresent(RENDERER);
}

static void display_set_orientation(int degrees) { display_refresh(); }

static void display_set_backlight(int val) { display_refresh(); }

const char* display_save(const char* prefix) {
    if (!RENDERER) {
        display_init();
    }
    static int count;
    static char filename[256];
    // take a cropped view of the screen contents
    const SDL_Rect rect = { 0, 0, DISPLAY_RESX, DISPLAY_RESY };
    SDL_Surface* crop = SDL_CreateRGBSurface(
        BUFFER->flags, rect.w, rect.h, BUFFER->format->BitsPerPixel,
        BUFFER->format->Rmask, BUFFER->format->Gmask, BUFFER->format->Bmask,
        BUFFER->format->Amask);
    SDL_BlitSurface(BUFFER, &rect, crop, NULL);
    // compare with previous screen, skip if equal
    if (PREV_SAVED != NULL) {
        if (memcmp(PREV_SAVED->pixels, crop->pixels, crop->pitch * crop->h) == 0) {
            SDL_FreeSurface(crop);
            return filename;
        }
        SDL_FreeSurface(PREV_SAVED);
    }
    // save to png
    snprintf(filename, sizeof(filename), "%s%08d.png", prefix, count++);
    IMG_SavePNG(crop, filename);
    PREV_SAVED = crop;
    return filename;
}

void display_clear_save(void) {
    SDL_FreeSurface(PREV_SAVED);
    PREV_SAVED = NULL;
}

bool display_toif_info(const uint8_t* data, uint32_t len, uint16_t* out_w,
    uint16_t* out_h, bool* out_grayscale) {
    if (len < 12 || memcmp(data, "TOI", 3) != 0) {
        return false;
    }
    bool grayscale = false;
    if (data[3] == 'f') {
        grayscale = false;
    }
    else if (data[3] == 'g') {
        grayscale = true;
    }
    else {
        return false;
    }

    uint16_t w = *(uint16_t*)(data + 4);
    uint16_t h = *(uint16_t*)(data + 6);

    uint32_t datalen = *(uint32_t*)(data + 8);
    if (datalen != len - 12) {
        return false;
    }

    if (out_w != NULL && out_h != NULL && out_grayscale != NULL) {
        *out_w = w;
        *out_h = h;
        *out_grayscale = grayscale;
    }
    return true;
}


static void uzlib_prepare(struct uzlib_uncomp* decomp, uint8_t* window,
    const void* src, uint32_t srcsize, void* dest,
    uint32_t destsize) {
    memset(decomp, 0x00,sizeof(struct uzlib_uncomp));
    if (window) {
        memset(window, 0x00,UZLIB_WINDOW_SIZE);
    }
    memset(dest, 0x00,destsize);
    decomp->source = (const uint8_t*)src;
    decomp->source_limit = decomp->source + srcsize;
    decomp->dest = (uint8_t*)dest;
    decomp->dest_limit = decomp->dest + destsize;
    uzlib_uncompress_init(decomp, window, window ? UZLIB_WINDOW_SIZE : 0);
}

static inline void clamp_coords(int x, int y, int w, int h, int* x0, int* y0,
    int* x1, int* y1) {
    *x0 = MAX(x, 0);
    *y0 = MAX(y, 0);
    *x1 = MIN(x + w - 1, DISPLAY_RESX - 1);
    *y1 = MIN(y + h - 1, DISPLAY_RESY - 1);
}


void display_image(int x, int y, int w, int h, const void* data,
    uint32_t datalen) {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    clamp_coords(x, y, w, h, &x0, &y0, &x1, &y1);
    display_set_window(x0, y0, x1, y1);
    x0 -= x;
    x1 -= x;
    y0 -= y;
    y1 -= y;

    struct uzlib_uncomp decomp = { 0 };
    uint8_t decomp_window[UZLIB_WINDOW_SIZE] = { 0 };
    uint8_t decomp_out[2] = { 0 };
    uzlib_prepare(&decomp, decomp_window, data, datalen, decomp_out,
        sizeof(decomp_out));

    for (uint32_t pos = 0; pos < w * h; pos++) {
        int st = uzlib_uncompress(&decomp);
        if (st == TINF_DONE) break;  // all OK
        if (st < 0) break;           // error
        const int px = pos % w;
        const int py = pos / w;
        if (px >= x0 && px <= x1 && py >= y0 && py <= y1) {
            PIXELDATA((decomp_out[0] << 8) | decomp_out[1]);
        }
        decomp.dest = (uint8_t*)&decomp_out;
    }
}

int display_backlight(int val) {
#if TREZOR_MODEL == 1
    val = 255;
#endif
    if (DISPLAY_BACKLIGHT != val && val >= 0 && val <= 255) {
        DISPLAY_BACKLIGHT = val;
        display_set_backlight(val);
    }
    return DISPLAY_BACKLIGHT;
}

static inline uint16_t interpolate_color(uint16_t color0, uint16_t color1,
    uint8_t step) {
    uint8_t cr = 0, cg = 0, cb = 0;
    cr = (((color0 & 0xF800) >> 11) * step +
        ((color1 & 0xF800) >> 11) * (15 - step)) /
        15;
    cg = (((color0 & 0x07E0) >> 5) * step +
        ((color1 & 0x07E0) >> 5) * (15 - step)) /
        15;
    cb = ((color0 & 0x001F) * step + (color1 & 0x001F) * (15 - step)) / 15;
    return (cr << 11) | (cg << 5) | cb;
}

static inline void set_color_table(uint16_t colortable[16], uint16_t fgcolor,
    uint16_t bgcolor) {
    for (int i = 0; i < 16; i++) {
        colortable[i] = interpolate_color(fgcolor, bgcolor, i);
    }
}

void display_icon(int x, int y, int w, int h, const void* data,
    uint32_t datalen, uint16_t fgcolor, uint16_t bgcolor) {
    x &= ~1;  // cannot draw at odd coordinate
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    clamp_coords(x, y, w, h, &x0, &y0, &x1, &y1);
    display_set_window(x0, y0, x1, y1);
    x0 -= x;
    x1 -= x;
    y0 -= y;
    y1 -= y;

    uint16_t colortable[16] = { 0 };
    set_color_table(colortable, fgcolor, bgcolor);

    struct uzlib_uncomp decomp = { 0 };
    uint8_t decomp_window[UZLIB_WINDOW_SIZE] = { 0 };
    uint8_t decomp_out = 0;
    uzlib_prepare(&decomp, decomp_window, data, datalen, &decomp_out,
        sizeof(decomp_out));

    for (uint32_t pos = 0; pos < w * h / 2; pos++) {
        int st = uzlib_uncompress(&decomp);
        if (st == TINF_DONE) break;  // all OK
        if (st < 0) break;           // error
        const int px = pos % w;
        const int py = pos / w;
        if (px >= x0 && px <= x1 && py >= y0 && py <= y1) {
            PIXELDATA(colortable[decomp_out >> 4]);
            PIXELDATA(colortable[decomp_out & 0x0F]);
        }
        decomp.dest = (uint8_t*)&decomp_out;
    }
}

void read_file(char* fname, char** buffer, long* lSize) {
    FILE* pFile;
    size_t result;

    pFile = fopen(fname, "rb");
    if (pFile == NULL) { fputs("File error", stderr); exit(1); }

    // obtain file size:
    fseek(pFile, 0, SEEK_END);
    *lSize = ftell(pFile);
    rewind(pFile);

    // allocate memory to contain the whole file:
    *buffer = (char*)malloc(sizeof(char) * (*lSize));
    if (*buffer == NULL) { fputs("Memory error", stderr); exit(2); }

    // copy the file into the buffer:
    result = fread(*buffer, 1, *lSize, pFile);
    if (result != *lSize) { fputs("Reading error", stderr); exit(3); }

    /* the whole file is now loaded in the memory buffer. */

    // terminate
    fclose(pFile);
    return;
}

// 3字节t，2字节l，N字节v
// v中: 2字节w，2字节h，4字节len，N字节data
void display_text(int x, int y, char* text) {
    size_t p = 0;
    size_t tp = 0;
    while (tp < strlen(text)) {
        while (p < xiangsu12_SIZE) {
            if (0 == memcmp(text + tp, xiangsu12 + p, 3)) {//utf8编码的中文是3个字节，不考虑4个字节的生僻字
                //找到了，开始解数据
                uint16_t w = *(uint16_t*)(xiangsu12 + p + 3 + 2);
                uint16_t h = *(uint16_t*)(xiangsu12 + p + 3 + 2 + 2);
                uint32_t datalen = *(uint32_t*)(xiangsu12 + p + 3 + 2 + 2 + 2);
                display_icon(x, y, w, h, xiangsu12 + p + 3 + 2 + 2 + 2 + 4, datalen, COLOR_WHITE, COLOR_BLACK);
                x = x + w;
                tp += 3;
                p = 0;
                if (tp == strlen(text)) {
                    return;//显示完了
                }
                continue;
            }
            else {
                //找字库中的下一个字
                p += 3;
                p += *(uint16_t*)(xiangsu12 + p);
                p += 2;
                continue;
            }
        }
        //字库里就没有这个字,先什么都不显示，回头可以显示一个框框
        p = 0;
        tp += 3;
    }

}

#include "images.h"
int main(int argc, char* argv[])
{

    uint16_t w = 0;
    uint16_t h = 0;
    bool grayscale = false;
    long lSize;
    //char* bufferImg = NULL;
    //read_file("../../../../../toif-convert/tests/book.toif",&bufferImg ,&lSize);
    //bool valid = display_toif_info(IMG_home, IMG_home_SIZE, &w, &h, &grayscale);
    //display_image(0, 0, w, h, IMG_home + 12, IMG_home_SIZE - 12);
    char* ch = u8"教程耿用使";
    display_text(0,0,ch);


    display_backlight(255);
    display_refresh();
    //free(bufferImg);

    /*
    char* bufferFont = NULL;
    read_file("../../../../../toif-convert/tests/font.toif", &bufferFont, &lSize);
    valid = display_toif_info(bufferFont, lSize, &w, &h, &grayscale);
    display_icon(0, 50, w, h, bufferFont + 12, lSize - 12, COLOR_WHITE, COLOR_BLACK);
    display_icon(0, 100, w, h, bufferFont + 12, lSize - 12, COLOR_RED, COLOR_BLACK);
    display_icon(0, 150, w, h, bufferFont + 12, lSize - 12, COLOR_GREEN, COLOR_BLACK);
    display_icon(0, 200, w, h, bufferFont + 12, lSize - 12, COLOR_BLUE, COLOR_BLACK);
    display_refresh();
    free(bufferFont);
    */

    display_save("save");

    bool quit = false;
    //Event handler
    SDL_Event e;

    while (!quit)
    {
        //Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            //User requests quit
            if (e.type == SDL_QUIT)
            {
                quit = true;
            }
        }


    }


    return 0;
}
