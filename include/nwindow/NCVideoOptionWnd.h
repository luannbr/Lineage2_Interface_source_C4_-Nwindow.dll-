#pragma once
// NCVideoOptionWnd - the video page of the options window (UClass "CVideoOptionWnd", 0x138 in C4):
// display mode and refresh rate (from the render device), gamma, texture / model / animation
// detail, shadows, ground detail, terrain and PC/NPC ranges, PC/NPC limit, reflections, the post
// processing kind, texture filter, anti-aliasing, the screen shot quality and "Lower Detail"
// (which fixes the detail controls low). Apply sets the globals and the render device and saves
// Option.ini ([Video], [ClippingRange], [L2WaterEffect], [Game]).
//
// C4 (read from OnCreate 0x100C99E0, OnDestroy, InitItems and LoadSettings): the color cursor box and the glow
// box are gone; a post processing combo (GL2PostProcessType, 0 .. 5; replaces the glow box) and a screen shot
// quality combo (GL2ScreenShotQuality) are new; the controls moved (the C3 layout + 0x20 does not hold).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCComboBox;
class NCCheckBox;

// NWM_COMMAND ids of the controls
enum
{
    NID_VIDEO_RESOLUTION = 0x1F08,
    NID_VIDEO_GAMMA = 0x1F09,
    NID_VIDEO_TEXTURE = 0x1F0A,
    NID_VIDEO_TERRAINRANGE = 0x1F0D,
    NID_VIDEO_PAWNRANGE = 0x1F0E,
    NID_VIDEO_REFLECTION = 0x1F11,
    NID_VIDEO_REFRESH = 0x1F14,
    NID_VIDEO_ACTORLIMIT = 0x1F17,
    NID_VIDEO_MODELING = 0x1F18,
    NID_VIDEO_ANIMATION = 0x1F19,
    NID_VIDEO_ANTIALIAS = 0x1F20,   // C3
    NID_VIDEO_SCREENSHOT = 0x1F24,   // C4 (names ours)
    NID_VIDEO_POSTPROCESS = 0x1F25,   // C4: the window reacts to its selection (OnPostProcessChanged)
    NID_VIDEO_LOWERDETAIL = (INT)0xF0F01465,
    NID_VIDEO_SHADOW = (INT)0xF0F01467,
    NID_VIDEO_GROUNDDETAIL = (INT)0xF0F01468,
    NID_VIDEO_TEXTUREFILTER = (INT)0xF0F01469,
    NID_VIDEO_FULLSCREEN = (INT)0xF0F0146A,   // phase 6 (NW_FEATURES): not in the stock page
};

class NCVideoOptionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCVideoOptionWnd, NCWnd, 0, 0x102ECCF8)

    NCComboBox* ResolutionCombo;           // +0xE8 "%d*%d %dbit", the device's display modes
    NCComboBox* RefreshCombo;              // +0xEC "%dHz", the rates of the selected mode
    NCComboBox* GammaCombo;                // +0xF0 "5" .. "1": gamma 1.2 .. 0.4
    NCComboBox* TextureCombo;              // +0xF4 High / Medium / Low (GL2TextureDetail)
    NCComboBox* ModelingCombo;             // +0xF8 High / Low (GL2UseReducePawnLOD)
    NCComboBox* AnimationCombo;            // +0xFC High / Low (GL2UseSkipAnim)
    NCCheckBox* ShadowBox;                 // +0x100 GIsL2Projector
    NCCheckBox* GroundDetailBox;           // +0x104 GL2RenderDeco
    NCComboBox* PostProcessCombo;          // +0x108 (new in C4, name ours) GL2PostProcessType (0 .. 5); needs the shaders 2.0
    NCComboBox* TerrainRangeCombo;         // +0x10C "5" .. "1": terrain clipping 8 .. 4
    NCComboBox* PawnRangeCombo;            // +0x110 "5" .. "1": PC/NPC clipping 3 .. 1
    NCComboBox* ReflectionCombo;           // +0x114 None / All / Sky (water effect type)
    NCCheckBox* LowerDetailBox;            // +0x118 GL2KeepMinFrameRate
    NCCheckBox* TextureFilterBox;          // +0x11C GL2UseTrilinear
    NCComboBox* ActorLimitCombo;           // +0x120 None / "5" .. "1" (GL2RenderActorLimitOpt)
    NCComboBox* AntialiasCombo;            // +0x124 GL2Antialiasing; None / 2x / 4x by GetMultiSample (off for post processing 3 .. 5)
    NCComboBox* ScreenShotCombo;           // +0x128 (new in C4, name ours) GL2ScreenShotQuality (four levels)
    NCWnd* UnusedD0[3];                    // +0x12C destroyed, never created (phase 6, NW_FEATURES:
                                           //   [0] is the "Full Screen" box, an NCCheckBox)

    NCVideoOptionWnd();                                              // 0x100cb2d0
    // ~NCVideoOptionWnd                                             // 0x100cb330

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100c3f90
    virtual INT OnCreate();                                          // 37 0x100c99e0
    virtual INT OnDestroy();                                         // 38 0x100c5190
    virtual void ShowWindow();                                       // 48 0x100c4210
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100c4b40
    // names ours
    virtual INT OnResolutionChanged(INT Index);                      // 74 0x100c5650
    virtual INT OnRefreshChanged(INT Index) { return 1; }            // 75 0x100f76f0 (folded)
    virtual void NewVirtual76();  // 76 0x100c4050 (only this client has it; defined in NNewVirtuals.gen.cpp): the body is OnPostProcessChanged
    virtual INT OnLowerDetailClicked();                              // 77 0x100c6d60

    // the body of slot 76, hooked by its address under this name (the slot's own definition is the generated stub of
    // NNewVirtuals.gen.cpp); the post processing kind 3 .. 5 takes the anti-aliasing out
    INT OnPostProcessChanged(INT Index);                             // 0x100c4050 (name ours, ret 4)

    // names ours
    void InitModes();                                                // 0x100c40a0
    void LoadSettings();                                             // 0x100c57a0 current values
    void Apply();                                                    // 0x100c5e70
    void ClearItems();                                               // 0x100c5580 (C2 only; before InitItems)
    void InitItems();                                                // 0x100c90c0

    // phase 6 (defined only with NW_FEATURES): the "Full Screen" box shows the mode the viewport
    // is in now (LoadSettings, and every time the options window opens)
    void SyncModeBox();
};
NW_ASSERT_OFFSET(NCVideoOptionWnd, ResolutionCombo, 0xE8);
NW_ASSERT_OFFSET(NCVideoOptionWnd, ShadowBox, 0x100);
NW_ASSERT_OFFSET(NCVideoOptionWnd, PostProcessCombo, 0x108);
NW_ASSERT_OFFSET(NCVideoOptionWnd, LowerDetailBox, 0x118);
NW_ASSERT_OFFSET(NCVideoOptionWnd, ActorLimitCombo, 0x120);
NW_ASSERT_OFFSET(NCVideoOptionWnd, AntialiasCombo, 0x124);
NW_ASSERT_OFFSET(NCVideoOptionWnd, ScreenShotCombo, 0x128);
NW_ASSERT_OFFSET(NCVideoOptionWnd, UnusedD0, 0x12C);
NW_ASSERT_SIZE(NCVideoOptionWnd, 0x138);

#pragma pack(pop)
