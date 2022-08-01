#ifndef MAIN_H
#define MAIN_H

struct RumbleData {
    u8 unk00;
    u8 unk01;
    s16 unk02;
    s16 unk04;
};

extern OSThread gIdleThread;
extern OSThread gMainThread;
extern OSThread gGameLoopThread;
extern OSThread gSoundThread;
#ifdef VERSION_SH
extern OSThread gRumblePakThread;

extern s32 gRumblePakPfs; // Actually an OSPfs but we don't have that header yet
#endif

extern OSMesgQueue gPIMesgQueue;
extern OSMesgQueue gIntrMesgQueue;
extern OSMesgQueue gSPTaskMesgQueue;
#ifdef VERSION_SH
extern OSMesgQueue gRumblePakSchedulerMesgQueue;
extern OSMesgQueue gRumbleThreadVIMesgQueue;
#endif
extern OSMesg gDmaMesgBuf[1];
extern OSMesg gPIMesgBuf[32];
extern OSMesg gSIEventMesgBuf[1];
extern OSMesg gIntrMesgBuf[16];
extern OSMesg gUnknownMesgBuf[16];
extern OSIoMesg gDmaIoMesg;
extern OSMesg D_80339BEC;
extern OSMesgQueue gDmaMesgQueue;
extern OSMesgQueue gSIEventMesgQueue;
#ifdef VERSION_SH
extern OSMesg gRumblePakSchedulerMesgBuf[1];
extern OSMesg gRumbleThreadVIMesgBuf[1];

extern struct RumbleData gRumbleDataQueue[3];
#endif

extern struct VblankHandler *gVblankHandler1;
extern struct VblankHandler *gVblankHandler2;
extern struct SPTask *gActiveSPTask;
extern u32 sNumVblanks;
extern s8 gResetTimer;
extern s8 D_8032C648;
extern s8 gDebugLevelSelect;
extern s8 D_8032C650;
extern s8 gShowProfiler;
extern s8 gShowDebugText;

void set_vblank_handler(s32 index, struct VblankHandler *handler, OSMesgQueue *queue, OSMesg *msg);
void dispatch_audio_sptask(struct SPTask *spTask);
void send_display_list(struct SPTask *spTask);

#endif // MAIN_H
