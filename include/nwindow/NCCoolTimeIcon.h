#pragma once
// NCCoolTimeIcon - the cool time drawn over a skill icon (name from its guard string; 0x48
// bytes, kept in the shortcut bar's slots and NCSkillItemWnd::CoolTimes). An animated
// "material" in the manner of UE2's: Start sets the time, the paint code asks GetTexture for
// the frame of now (360 "L2_SkillTime.Rotation_Type.cooltime<n>" textures), and when the time
// is over the icon flashes for FlashTime seconds (9 "L2_SkillTime.CooltimeEnd.CooltimeEnd00<n>"
// frames, GetFlashTexture) with a sound (bSoundDue, played by the window).

#include "nwindow/NWindow.h"

// the frames, loaded by the first icon constructed (names ours)
#if NW_HOOK_MODE
#define GCoolTimeTextures    NW_GLOBAL(TArray<UTexture*>, 0x102C1FC4)
#define GCoolTimeEndTextures NW_GLOBAL(TArray<UTexture*>, 0x102C1FD0)
#else
extern TArray<UTexture*> GCoolTimeTextures;
extern TArray<UTexture*> GCoolTimeEndTextures;
#endif

// 4-byte packing: the shortcut slot embeds one at +0x204
#pragma pack(push, 4)
class NCCoolTimeIcon
{
public:
    UTexture* Texture;         // +0x00 the frame PreSetMaterial picked
    INT Unknown04;             // +0x04
    DWORD Flags;               // +0x08 bit 0: the frames loop, bit 1: frozen (both cleared by
                               //   the constructor, set nowhere in NWindow)
    FLOAT Time;                // +0x0C seconds into the frames
    INT Unknown10[2];          // +0x10
    DOUBLE Duration;           // +0x18 seconds the frames last
    DOUBLE FrameTime;          // +0x20 seconds per frame
    DOUBLE StartTime;          // +0x28 appSeconds() at the start
    DOUBLE FlashTime;          // +0x30 (0.5)
    INT StartFrame;            // +0x38 frames already gone (a cool time that began earlier)
    UBOOL bRunning;            // +0x3C
    UBOOL bFlashing;           // +0x40 the cool time just ended
    UBOOL bSoundDue;           // +0x44 the end sound is still to be played

    NCCoolTimeIcon();                                                // 0x100173c0
    ~NCCoolTimeIcon() {}                                             // 0x10045870 (an empty
                                                                     //   body many share)
    // names ours
    void SetTime(DOUBLE Total, DOUBLE Remaining);                    // 0x10016730
    void Start(DOUBLE Total, DOUBLE Remaining);                      // 0x10016800 (the console)
    void Stop();                                                     // 0x10016830
    UBOOL IsIdle();                                                  // 0x10016840
    void StartFlash(DOUBLE Seconds);                                 // 0x10016850
    void EndCoolTime();                                              // 0x100170e0
    void PreSetMaterial(DOUBLE TimeSeconds);                         // 0x10017110 (guard name)
    UTexture* GetFlashTexture();                                     // 0x100172a0 NULL when over
    UTexture* GetTexture();                                          // 0x10017550 NULL when over
    DOUBLE TotalTime;          // +0x48 new in C3 (name ours): the Total of Start; PreSetMaterial subtracts the frames' sum
                               //   from it into RemainingTime; Stop, StartFlash and EndCoolTime clear it
    DOUBLE RemainingTime;      // +0x50 new in C3 (name ours): seconds left of the cool time (0 when idle)
};
#pragma pack(pop)
NW_ASSERT_OFFSET(NCCoolTimeIcon, Duration, 0x18);
NW_ASSERT_OFFSET(NCCoolTimeIcon, StartFrame, 0x38);
NW_ASSERT_OFFSET(NCCoolTimeIcon, bSoundDue, 0x44);
NW_ASSERT_OFFSET(NCCoolTimeIcon, TotalTime, 0x48);
NW_ASSERT_OFFSET(NCCoolTimeIcon, RemainingTime, 0x50);
NW_ASSERT_SIZE(NCCoolTimeIcon, 0x58);
