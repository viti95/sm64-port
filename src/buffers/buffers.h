#ifndef BUFFERS_H
#define BUFFERS_H

#include <PR/ultratypes.h>

#include "game/save_file.h"
#include "game/game_init.h"

extern u8 gDecompressionHeap[];

extern u8 gAudioHeap[];

extern u8 gAudioSPTaskYieldBuffer[];

extern u8 gIdleThreadStack[];
extern u8 gThread3Stack[];
extern u8 gThread4Stack[];
extern u8 gThread5Stack[];
#ifdef VERSION_SH
extern u8 gThread6Stack[];
#endif

extern u8 gGfxSPTaskYieldBuffer[];

extern struct SaveBuffer gSaveBuffer;

extern u8 gGfxSPTaskStack[];

#ifdef TARGET_N64
#define GFX_NUM_POOLS 2
#else
#define GFX_NUM_POOLS 1
#endif
extern struct GfxPool gGfxPools[GFX_NUM_POOLS];

#endif // BUFFERS_H
