#ifndef GFX_DOS_API_H
#define GFX_DOS_API_H

#include "gfx_window_manager_api.h"

extern struct GfxWindowManagerAPI gfx_dos_api;

#define VM_13H 1
#define VM_X 2
#define VM_VESA_LFB_8 3
#define VM_VESA_LFB_15 4
#define VM_VESA_LFB_16 5
#define VM_VESA_LFB_24 6
#define VM_VESA_LFB_32 7
#define VM_HERCULES 8

#endif
