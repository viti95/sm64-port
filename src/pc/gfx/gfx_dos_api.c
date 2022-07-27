#ifdef TARGET_DOS

// dithering and palette construction code taken from
// https://bisqwit.iki.fi/jutut/kuvat/programming_examples/walk.cc

#include "macros.h"
#include "gfx_dos_api.h"
#include "gfx_opengl.h"
#include "../configfile.h"
#include "../common.h"

#include <dos.h>
#include <pc.h>
#include <go32.h>
#include <stdio.h>
#include <math.h>
#include <sys/farptr.h>
#include <sys/nearptr.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <stdarg.h>
#include <allegro.h>
#include <dpmi.h>

#ifdef ENABLE_DMESA

#include <GL/gl.h>
#include <GL/dmesa.h>

static DMesaVisual dv;
static DMesaContext dc;
static DMesaBuffer db;

static void gfx_dos_init_impl(void) {
    dv = DMesaCreateVisual(configScreenWidth, configScreenHeight, 16, 60, 1, 1, 0, 16, 0, 0);
    if (!dv) {
        printf("DMesaCreateVisual failed: resolution not supported?\n");
        abort();
    }

    dc = DMesaCreateContext(dv, NULL);
    if (!dc) {
        printf("DMesaCreateContext failed\n");
        abort();
    }

    db = DMesaCreateBuffer(dv, 0, 0, configScreenWidth, configScreenHeight);
    if (!db) {
        printf("DMesaCreateBuffer failed\n");
        abort();
    }

    DMesaMakeCurrent(dc, db);
}

static void gfx_dos_shutdown_impl(void) {
    DMesaMakeCurrent(NULL, NULL);
    DMesaDestroyBuffer(db);
    db = NULL;
    DMesaDestroyContext(dc);
    dc = NULL;
    DMesaDestroyVisual(dv);
    dv = NULL;
}

#else // ENABLE_FXMESA

#define REG_SELECT 0x3C4 /* VGA register select */
#define REG_VALUE 0x3C5  /* send value to selected register */
#define REG_MASK 0x02    /* map mask register */
#define PAL_LOAD 0x3C8   /* start loading palette */
#define PAL_COLOR 0x3C9  /* load next palette color */

#define SCREEN_WIDTH 320      /* width in pixels */
#define SCREEN_HEIGHT_200 200 /* height in mode 13h, in pixels */
#define SCREEN_HEIGHT_240 240 /* height in mode X, in pixels */
#define SCREEN_WIDTH_2X 640
#define SCREEN_HEIGHT_200_2X 400
#define SCREEN_HEIGHT_240_2X 480

// 7*9*4 regular palette (252 colors)
#define PAL_RBITS 7
#define PAL_GBITS 9
#define PAL_BBITS 4
#define PAL_GAMMA 1.5 // apply this gamma to palette

#define DIT_GAMMA (2.0 / PAL_GAMMA) // apply this gamma to dithering
#define DIT_BITS 6                  // dithering strength

#define VGA_BASE 0xA0000
#define HERCULES_BASE 0xB0000

#define umin(a, b) (((a) < (b)) ? (a) : (b))

typedef struct {
    uint8_t r, g, b, a;
} RGBA;

static uint8_t rgbconv[3][256][256];
static uint8_t dit_kernel[8][8];
static uint8_t dit_kernel_mode13h[320 * 200];
static uint8_t Graph_640x400[12] = { 0x03, 0x34, 0x28, 0x2A, 0x47, 0x69, 0x00, 0x64, 0x65, 0x02, 0x03, 0x0A };
static uint16_t dit_kernel_hercules_4x4[16] = {  45, 405, 135, 495, 585, 225, 675, 315, 180, 540, 90, 450, 720, 360, 630, 270 };

static uint8_t hercules_backbuffer[640 * 400 / 8];

#ifdef ENABLE_OSMESA
#include <osmesa.h>
OSMesaContext ctx;
uint32_t *osmesa_buffer; // 320x240x3 bytes (RGB)
#define GFX_BUFFER osmesa_buffer
#else
#include "gfx_soft.h"
#define GFX_BUFFER gfx_output
#endif

uint8_t *ptrscreen;
uint32_t numLoops;
void (*backbuffer_function)(void);

static void gfx_dos_swap_buffers_modex_dither(void) {
    // we're gonna be only sending plane switch commands until the end of the function
    outportb(REG_SELECT, REG_MASK);
    register const RGBA *inp;
    uint8_t *outp;
    register unsigned d;
    // the pixels go 0 1 2 3 0 1 2 3, so we can't afford switching planes every pixel
    // instead we go (switch) 0 0 0 ... (switch) 1 1 1 ...
    for (unsigned plane = 0; plane < 4; ++plane) {
        outportb(REG_VALUE, 1 << plane);
        for (register unsigned x = plane; x < SCREEN_WIDTH; x += 4) {
            inp = (RGBA *) (GFX_BUFFER + x);
            // target pixel is at VGAMEM[(y << 4) + (y << 6) + (x >> 2)]
            // calculate the x part and then just add 16 + 64 until bottom
            outp = ptrscreen + (x >> 2);
            for (register unsigned y = 0; y < SCREEN_HEIGHT_240;
                 ++y, inp += SCREEN_WIDTH, outp += (1 << 4) + (1 << 6)) {
                d = dit_kernel[y & 7][x & 7];
                *outp = rgbconv[2][inp->r][d] + rgbconv[1][inp->g][d] + rgbconv[0][inp->b][d];
            }
        }
    }
}

static void gfx_dos_swap_buffers_modex(void) {
    // we're gonna be only sending plane switch commands until the end of the function
    outportb(REG_SELECT, REG_MASK);
    uint8_t *inp = GFX_BUFFER;
    uint8_t *outp;
    // the pixels go 0 1 2 3 0 1 2 3, so we can't afford switching planes every pixel
    // instead we go (switch) 0 0 0 ... (switch) 1 1 1 ...
    for (unsigned plane = 0; plane < 4; ++plane) {
        outportb(REG_VALUE, 1 << plane);
        for (register unsigned x = plane; x < SCREEN_WIDTH; x += 4) {
            inp = (uint8_t *) (GFX_BUFFER) + 3*x;
            // target pixel is at VGAMEM[(y << 4) + (y << 6) + (x >> 2)]
            // calculate the x part and then just add 16 + 64 until bottom
            outp = ptrscreen + (x >> 2);
            for (register unsigned y = 0; y < SCREEN_HEIGHT_240; ++y, inp += SCREEN_WIDTH * 3, outp += (1 << 4) + (1 << 6)) {
                uint8_t R = (*(inp) & 0b11100000);
                uint8_t G = (*(inp + 1) & 0b11100000) >> 3;
                uint8_t B = *(inp + 2) >> 6;
                *outp = R | G | B;
            }
        }
    }
}

static void gfx_dos_swap_buffers_mode13_dither(void) {
    const RGBA *inp = (RGBA *) GFX_BUFFER;
    uint8_t *vram = ptrscreen;

    for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT_200; i++, inp++, vram++) {
        uint8_t d = dit_kernel_mode13h[i];
        *vram = rgbconv[2][inp->r][d] + rgbconv[1][inp->g][d] + rgbconv[0][inp->b][d];
    }
}

static void gfx_dos_swap_buffers_mode13(void) {

    uint8_t *inp = GFX_BUFFER;
    uint16_t *vram = ptrscreen;

    for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT_200 / 2; i++, inp += 6, vram++) {
        uint16_t R1 = (*(inp) & 0b11100000);
        uint16_t G1 = (*(inp + 1) & 0b11100000) >> 3;
        uint16_t B1 = *(inp + 2) >> 6;
        uint16_t R2 = (*(inp + 3) & 0b11100000) << 8;
        uint16_t G2 = (*(inp + 4) & 0b11100000) << 5;
        uint16_t B2 = (*(inp + 5) & 0b11000000) << 2;

        *vram = R1 | G1 | B1 | R2 | G2 | B2;
    }

}

static void gfx_dos_swap_buffers_hercules(void) {
    const RGBA *inp = (RGBA *) GFX_BUFFER;
    uint8_t *vram = (uint8_t *) ptrscreen;
    uint32_t backbuffer_position = 0;
    uint8_t position = 0;

    for (unsigned y = 0; y < SCREEN_HEIGHT_200_2X; y++) {
        for (unsigned x = 0; x < SCREEN_WIDTH_2X / 8; x++, vram++, backbuffer_position++) {
            uint8_t value = 0;

            for (unsigned i = 0; i < 8; i++) {

                uint32_t buffer_pos = (y * 240 / 400) * 320 + (x * 4) + (i / 2) ;

                RGBA *inps = inp + buffer_pos;

                uint8_t dither_pos_4x4 = ((y & 3) << 2) | (i & 3);

                uint16_t sum = (uint16_t) inps->r + (uint16_t) inps->g + (uint16_t) inps->b;

                if (sum > dit_kernel_hercules_4x4[dither_pos_4x4]) {
                    value |= (0x80 >> i);
                }
            }

            if (hercules_backbuffer[backbuffer_position] != value){
                    *vram = value;
                    hercules_backbuffer[backbuffer_position] = value;
            }

        }

        position++;

        if (position == 4) {
            vram -= 0x6000;
            position = 0;
        } else {
            vram += 0x2000 - 80;
        }
    }
}

static void gfx_dos_swap_buffers_vesa_lfb_15_native(void) {
    uint8_t *inp = GFX_BUFFER;
    uint32_t *vram = (uint32_t *) ptrscreen;

    uint16_t position = 0;

    for (unsigned i = 0; i < numLoops; i++, inp += 3, vram++) {
        uint32_t R = (*(inp) & 0b11111000) << 7;
        uint32_t G = (*(inp + 1) & 0b11111000) << 2;
        uint32_t B = *(inp + 2) >> 3;

        R |= R << 16;
        G |= G << 16;
        B |= B << 16;

        uint32_t value = R | G | B;

        *vram = value;
        *(vram + 320) = value;

        position++;

        if (position == 320){
            position = 0;
            vram += 320;
        }
    }
}

static void gfx_dos_swap_buffers_vesa_lfb_15(void) {
    uint8_t *inp = GFX_BUFFER;
    uint32_t *vram = (uint32_t *) ptrscreen;

    for (unsigned i = 0; i < numLoops; i++, inp += 6, vram++) {
        uint32_t R1 = (*(inp) & 0b11111000) << 7;
        uint32_t G1 = (*(inp + 1) & 0b11111000) << 2;
        uint32_t B1 = *(inp + 2) >> 3;
        uint32_t R2 = (*(inp + 3) & 0b11111000) << 23;
        uint32_t G2 = (*(inp + 4) & 0b11111000) << 18;
        uint32_t B2 = (*(inp + 5) & 0b11111000) << 13;

        *vram = R1 | G1 | B1 | R2 | G2 | B2;
    }
}

static void gfx_dos_swap_buffers_vesa_lfb_16_native(void) {
    uint8_t *inp = GFX_BUFFER;
    uint32_t *vram = (uint32_t *) ptrscreen;

    uint16_t position = 0;

    for (unsigned i = 0; i < numLoops; i++, inp += 3, vram++) {
        uint32_t R = (*(inp) & 0b11111000) << 8;
        uint32_t G = (*(inp + 1) & 0b11111100) << 3;
        uint32_t B = *(inp + 2) >> 3;

        R |= R << 16;
        G |= G << 16;
        B |= B << 16;

        uint32_t value = R | G | B;

        *vram = value;
        *(vram + 320) = value;

        position++;

        if (position == 320){
            position = 0;
            vram += 320;
        }
    }
}

static void gfx_dos_swap_buffers_vesa_lfb_16(void) {
    uint8_t *inp = GFX_BUFFER;
    uint32_t *vram = (uint32_t *) ptrscreen;

    for (unsigned i = 0; i < numLoops; i++, inp += 6, vram++) {
        uint32_t R1 = (*(inp) & 0b11111000) << 8;
        uint32_t G1 = (*(inp + 1) & 0b11111100) << 3;
        uint32_t B1 = *(inp + 2) >> 3;
        uint32_t R2 = (*(inp + 3) & 0b11111000) << 24;
        uint32_t G2 = (*(inp + 4) & 0b11111100) << 19;
        uint32_t B2 = (*(inp + 5) & 0b11111000) << 13;
        *vram = R1 | G1 | B1 | R2 | G2 | B2;
    }
}

static void gfx_dos_swap_buffers_vesa_lfb_24_native(void) {

    uint8_t *inp = GFX_BUFFER;
    uint8_t *vram = (uint8_t *) ptrscreen;

    uint16_t position = 0;

    for (unsigned i = 0; i < numLoops; i++, inp += 3, vram += 6) {

        *vram = *(inp);
        *(vram + 3) = *(inp);
        *(vram + 640 * 3) = *(inp);
        *(vram + 640 * 3 + 3) = *(inp);

        *(vram + 1) = *(inp + 1);
        *(vram + 4) = *(inp + 1);
        *(vram + 640 * 3 + 1) = *(inp + 1);
        *(vram + 640 * 3 + 4) = *(inp + 1);

        *(vram + 2) = *(inp + 2);
        *(vram + 5) = *(inp + 2);
        *(vram + 640 * 3 + 2) = *(inp + 2);
        *(vram + 640 * 3 + 5) = *(inp + 2);

        position++;

        if (position == 320){
            position = 0;
            vram += 640 * 3;
        }
    }

}

static void gfx_dos_swap_buffers_vesa_lfb_24(void) {
    uint32_t *inp = GFX_BUFFER;
    uint32_t *vram = (uint32_t *) ptrscreen;

    for (unsigned i = 0; i < numLoops; i++, inp++, vram++) {
        *vram = *inp;
    }
}

static void gfx_dos_swap_buffers_vesa_lfb_32_native(void) {
    uint32_t *inp = GFX_BUFFER;
    uint32_t *vram = (uint32_t *) ptrscreen;

    uint16_t position = 0;

    for (unsigned i = 0; i < numLoops; i++, inp++, vram += 2) {
        *vram = *inp;
        *(vram + 1) = *inp;
        *(vram + 640) = *inp;
        *(vram + 641) = *inp;

        position++;

        if (position == 320){
            position = 0;
            vram += 640;
        }
    }
}

static void gfx_dos_swap_buffers_vesa_lfb_32(void) {
    uint32_t *inp = GFX_BUFFER;
    uint32_t *vram = (uint32_t *) ptrscreen;

    for (unsigned i = 0; i < numLoops; i++, inp++, vram++) {
        *vram = *inp;
    }
}

static void gfx_dos_init_impl(void) {

    // create Bayer 8x8 dithering matrix
    for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < 8; ++x) {
            dit_kernel[y][x] = ((x) &4) / 4u + ((x) &2) * 2u + ((x) &1) * 16u + ((x ^ y) & 4) / 2u + ((x ^ y) & 2) * 4u + ((x ^ y) & 1) * 32u;
            dit_kernel[y][x] = (dit_kernel[y][x] & (0x3F - (0x3F >> DIT_BITS))) << 2;
        }

    // optimize Bayer 8x8 dithering matrix access (mode 13H)
    for (unsigned i = 0; i < 320 * 200; i++) {
        dit_kernel_mode13h[i] = dit_kernel[(i / 320) & 7][i & 7];
    }

    // create gamma-corrected look-up tables for dithering
    double dtab[256], ptab[256];
    for (unsigned n = 0; n < 256; ++n) {
        dtab[n] = (255.0 / 256.0) - pow(n / 256.0, 1.0 / DIT_GAMMA);
        ptab[n] = pow(n / 255.0, 1.0 / PAL_GAMMA);
    }

    for (unsigned n = 0; n < 256; ++n) {
        for (unsigned d = 0; d < 256; ++d) {
            rgbconv[0][n][d] = umin(PAL_BBITS - 1, (unsigned) (ptab[n] * (PAL_BBITS - 1) + dtab[d]));
            rgbconv[1][n][d] = PAL_BBITS * umin(PAL_GBITS - 1, (unsigned) (ptab[n] * (PAL_GBITS - 1) + dtab[d]));
            rgbconv[2][n][d] = PAL_GBITS * PAL_BBITS * umin(PAL_RBITS - 1, (unsigned) (ptab[n] * (PAL_RBITS - 1) + dtab[d]));
        }
    }

    unsigned long screen_base_addr;

    switch (configVideomode) {
        case VM_13H:

            configScreenWidth = SCREEN_WIDTH;
            configScreenHeight = SCREEN_HEIGHT_200;
            set_color_depth(8);
            set_gfx_mode(GFX_VGA, SCREEN_WIDTH, SCREEN_HEIGHT_200, 0, 0);
            ptrscreen = VGA_BASE + __djgpp_conventional_base;

            if (configDither)
                backbuffer_function = gfx_dos_swap_buffers_mode13_dither;
            else
                backbuffer_function = gfx_dos_swap_buffers_mode13;

            break;

        case VM_X:

            configScreenWidth = SCREEN_WIDTH;
            configScreenHeight = SCREEN_HEIGHT_240;
            set_color_depth(8);
            set_gfx_mode(GFX_MODEX, SCREEN_WIDTH, SCREEN_HEIGHT_240, 0, 0);
            ptrscreen = VGA_BASE + __djgpp_conventional_base;

            if (configDither)
                backbuffer_function = gfx_dos_swap_buffers_modex_dither;
            else
                backbuffer_function = gfx_dos_swap_buffers_modex;

            break;

        case VM_VESA_LFB_15:

            set_color_depth(15);

            if (configNativeResolution) {
                configScreenWidth = SCREEN_WIDTH;
                configScreenHeight = SCREEN_HEIGHT_240;
                set_gfx_mode(GFX_VESA2L, SCREEN_WIDTH_2X, SCREEN_HEIGHT_240_2X, 0, 0);
            }else{
                set_gfx_mode(GFX_VESA2L, configScreenWidth, configScreenHeight, 0, 0);
            }

            scroll_screen(0, 0);

            __dpmi_get_segment_base_address(screen->seg, &screen_base_addr);

            ptrscreen = (uint8_t *) (screen_base_addr + screen->line[0] - __djgpp_base_address);

            if (configNativeResolution){
                numLoops = configScreenWidth * configScreenHeight;
                backbuffer_function = gfx_dos_swap_buffers_vesa_lfb_15_native;
            }else{
                numLoops = (configScreenWidth * configScreenHeight) / 2;
                backbuffer_function = gfx_dos_swap_buffers_vesa_lfb_15;
            }

            break;

        case VM_VESA_LFB_16:

            set_color_depth(16);

            if (configNativeResolution) {
                configScreenWidth = SCREEN_WIDTH;
                configScreenHeight = SCREEN_HEIGHT_240;
                set_gfx_mode(GFX_VESA2L, SCREEN_WIDTH_2X, SCREEN_HEIGHT_240_2X, 0, 0);
            }else{
                set_gfx_mode(GFX_VESA2L, configScreenWidth, configScreenHeight, 0, 0);
            }

            scroll_screen(0, 0);

            __dpmi_get_segment_base_address(screen->seg, &screen_base_addr);

            ptrscreen = (uint8_t *) (screen_base_addr + screen->line[0] - __djgpp_base_address);

            if (configNativeResolution){
                numLoops = configScreenWidth * configScreenHeight;
                backbuffer_function = gfx_dos_swap_buffers_vesa_lfb_16_native;
            }else{
                numLoops = (configScreenWidth * configScreenHeight) / 2;
                backbuffer_function = gfx_dos_swap_buffers_vesa_lfb_16;
            }

            break;

        case VM_VESA_LFB_24:

            set_color_depth(24);

            if (configNativeResolution) {
                configScreenWidth = SCREEN_WIDTH;
                configScreenHeight = SCREEN_HEIGHT_240;
                set_gfx_mode(GFX_VESA2L, SCREEN_WIDTH_2X, SCREEN_HEIGHT_240_2X, 0, 0);
            }else{
                set_gfx_mode(GFX_VESA2L, configScreenWidth, configScreenHeight, 0, 0);
            }

            scroll_screen(0, 0);

            __dpmi_get_segment_base_address(screen->seg, &screen_base_addr);

            ptrscreen = (uint8_t *) (screen_base_addr + screen->line[0] - __djgpp_base_address);

            if (configNativeResolution){
                numLoops = configScreenWidth * configScreenHeight;
                backbuffer_function = gfx_dos_swap_buffers_vesa_lfb_24_native;
            }else{
                numLoops = (configScreenWidth * configScreenHeight * 3) / 4;
                backbuffer_function = gfx_dos_swap_buffers_vesa_lfb_24;
            }

            break;

        case VM_VESA_LFB_32:

            set_color_depth(32);

            if (configNativeResolution) {
                configScreenWidth = SCREEN_WIDTH;
                configScreenHeight = SCREEN_HEIGHT_240;
                set_gfx_mode(GFX_VESA2L, SCREEN_WIDTH_2X, SCREEN_HEIGHT_240_2X, 0, 0);
            }else{
                set_gfx_mode(GFX_VESA2L, configScreenWidth, configScreenHeight, 0, 0);
            }

            scroll_screen(0, 0);

            __dpmi_get_segment_base_address(screen->seg, &screen_base_addr);

            ptrscreen = (uint8_t *) (screen_base_addr + screen->line[0] - __djgpp_base_address);


            if (configNativeResolution){
                numLoops = configScreenWidth * configScreenHeight;
                backbuffer_function = gfx_dos_swap_buffers_vesa_lfb_32_native;
            }else{
                numLoops = configScreenWidth * configScreenHeight;
                backbuffer_function = gfx_dos_swap_buffers_vesa_lfb_32;
            }

            break;

        case VM_HERCULES:

            configScreenWidth = SCREEN_WIDTH;
            configScreenHeight = SCREEN_HEIGHT_240;
            ptrscreen = HERCULES_BASE + __djgpp_conventional_base;

            outportb(0x03BF, Graph_640x400[0]);
            for (int i = 0; i < 10; i++) {
                outportb(0x03B4, i);
                outportb(0x03B5, Graph_640x400[i + 1]);
            }
            outportb(0x03B8, Graph_640x400[11]);

            memset(ptrscreen, 0, 65536); // Clean 64kb
            memset(hercules_backbuffer, 0, 640 * 400 / 8);

            backbuffer_function = gfx_dos_swap_buffers_hercules;
            break;
    }

    if (configVideomode == VM_13H || configVideomode == VM_X) {

        outportb(PAL_LOAD, 0);

        if (configDither){
            // set up regular palette as configured above;
            // however, bias the colors towards darker ones in an exponential curve

            for (unsigned color = 0; color < PAL_RBITS * PAL_GBITS * PAL_BBITS; ++color) {
                outportb(PAL_COLOR,
                         pow(((color / (PAL_BBITS * PAL_GBITS)) % PAL_RBITS) * 1.0 / (PAL_RBITS - 1), PAL_GAMMA) * 63);
                outportb(PAL_COLOR,
                         pow(((color / (PAL_BBITS)) % PAL_GBITS) * 1.0 / (PAL_GBITS - 1), PAL_GAMMA) * 63);
                outportb(PAL_COLOR,
                         pow(((color) % PAL_BBITS) * 1.0 / (PAL_BBITS - 1), PAL_GAMMA) * 63);
            }
        } else {

            // set palette to RGB332

            for (unsigned color = 0; color < 256; color++) {

                unsigned int Rcolor;
                unsigned int Gcolor;
                unsigned int Bcolor;

                Rcolor = color >> 5;
                Gcolor = (color & 0x1C) >> 2;
                Bcolor = (color & 0x03);

                Rcolor = Rcolor << 3;
                Gcolor = Gcolor << 3;
                Bcolor = Bcolor << 4;

                outportb(PAL_COLOR, Rcolor);
                outportb(PAL_COLOR, Gcolor);
                outportb(PAL_COLOR, Bcolor);

            }

        }

    }

#ifdef ENABLE_OSMESA

    switch (configVideomode){
        case VM_13H:
        case VM_VESA_LFB_15:
        case VM_VESA_LFB_16:
        case VM_VESA_LFB_24:
            osmesa_buffer = (void *) malloc(configScreenWidth * configScreenHeight * 3 * sizeof(GLubyte));
            break;
        default:
            osmesa_buffer = (void *) malloc(configScreenWidth * configScreenHeight * 4 * sizeof(GLubyte));
            break;
    }

    if (!osmesa_buffer) {
        fprintf(stderr, "osmesa_buffer malloc failed!\n");
        abort();
    }

    switch (configVideomode){
        case VM_X:
            if (configDither){
                    ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
            }else{
                    ctx = OSMesaCreateContextExt(OSMESA_RGB, 16, 0, 0, NULL);
            }
            break;
        case VM_13H:
        case VM_VESA_LFB_15:
        case VM_VESA_LFB_16:
            ctx = OSMesaCreateContextExt(OSMESA_RGB, 16, 0, 0, NULL);
            break;
        case VM_VESA_LFB_24:
            ctx = OSMesaCreateContextExt(OSMESA_BGR, 16, 0, 0, NULL);
            break;
        case VM_VESA_LFB_32:
            ctx = OSMesaCreateContextExt(OSMESA_BGRA, 16, 0, 0, NULL);
            break;
        default:
            ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
            break;
    }

    if (!OSMesaMakeCurrent(ctx, osmesa_buffer, GL_UNSIGNED_BYTE, configScreenWidth, configScreenHeight)) {
        fprintf(stderr, "OSMesaMakeCurrent failed!\n");
        abort();
    }
    OSMesaPixelStore(OSMESA_Y_UP, GL_FALSE);
#endif
}

static void gfx_dos_shutdown_impl(void) {
#ifdef ENABLE_OSMESA
    OSMesaDestroyContext(ctx);
    free(osmesa_buffer);
    osmesa_buffer = NULL;
#endif
    // go back to default text mode
    set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
}

#endif // ENABLE_FXMESA

#ifdef VERSION_EU
#define FRAMERATE 25
#define FRAMETIME 40
#else
#define FRAMERATE 30
#define FRAMETIME 33
#endif

static bool init_done = false;
static bool do_render = true;
static volatile uint32_t tick = 0;
static uint32_t last = 0;

static void timer_handler(void) {
    ++tick;
}
END_OF_FUNCTION(timer_handler)

static void gfx_dos_init(UNUSED const char *game_name, UNUSED bool start_in_fullscreen) {
    if (__djgpp_nearptr_enable() == 0) {
        printf("Could get access to first 640K of memory.\n");
        abort();
    }

    allegro_init();

    LOCK_VARIABLE(tick);
    LOCK_FUNCTION(timer_handler);
    install_timer();
    install_int(timer_handler, FRAMETIME);

    gfx_dos_init_impl();

    last = tick;

    init_done = true;
}

static void gfx_dos_set_keyboard_callbacks(UNUSED bool (*on_key_down)(int scancode),
                                           UNUSED bool (*on_key_up)(int scancode),
                                           UNUSED void (*on_all_keys_up)(void)) {
}

static void
gfx_dos_set_fullscreen_changed_callback(UNUSED void (*on_fullscreen_changed)(bool is_now_fullscreen)) {
}

static void gfx_dos_set_fullscreen(UNUSED bool enable) {
}

static void gfx_dos_main_loop(void (*run_one_game_iter)(void)) {
    const uint32_t now = tick;

    const uint32_t frames = now - last;
    if (frames) {
        // catch up but skip the first FRAMESKIP frames
        int skip = (frames > configFrameskip) ? configFrameskip : (frames - 1);
        for (uint32_t f = 0; f < frames; ++f, --skip) {
            do_render = (skip <= 0);
            run_one_game_iter();
        }
        last = now;
    }
}

static void gfx_dos_get_dimensions(uint32_t *width, uint32_t *height) {
    *width = configScreenWidth;
    *height = configScreenHeight;
}

static void gfx_dos_handle_events(void) {
}

static bool gfx_dos_start_frame(void) {
    return do_render;
}

static void gfx_dos_swap_buffers_begin(void) {
#ifdef ENABLE_DMESA
    DMesaSwapBuffers(db);
#else
    backbuffer_function();
#endif
}

static void gfx_dos_swap_buffers_end(void) {
}

static double gfx_dos_get_time(void) {
    return 0.0;
}

static void gfx_dos_shutdown(void) {
    if (!init_done)
        return;

    gfx_dos_shutdown_impl();
    // allegro_exit() should be in atexit()

    init_done = false;
}

struct GfxWindowManagerAPI gfx_dos_api = { gfx_dos_init,
                                           gfx_dos_set_keyboard_callbacks,
                                           gfx_dos_set_fullscreen_changed_callback,
                                           gfx_dos_set_fullscreen,
                                           gfx_dos_main_loop,
                                           gfx_dos_get_dimensions,
                                           gfx_dos_handle_events,
                                           gfx_dos_start_frame,
                                           gfx_dos_swap_buffers_begin,
                                           gfx_dos_swap_buffers_end,
                                           gfx_dos_get_time,
                                           gfx_dos_shutdown };

#endif
