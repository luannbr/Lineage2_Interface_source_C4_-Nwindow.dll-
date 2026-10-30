#pragma once
// NCAudioOptionWnd - the audio page of the options window (UClass "CAudioOptionWnd", 0xA8):
// sound effect and music volume, each a combo box of six levels ("5" .. "1", Off = sysstring
// 228). Without an audio subsystem both boxes are disabled. Apply sets the volumes and saves
// them (Option.ini [Audio] SoundVolume / MusicVolume); music turned back on from Off restarts.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCComboBox;

// NWM_COMMAND ids of the boxes
enum
{
    NID_AUDIO_SOUND = 0x1F12,
    NID_AUDIO_MUSIC = 0x1F16,
    NID_AUDIO_WAVVOICE = 0x1F22,   // new in C3 (name ours)
    NID_AUDIO_OGGVOICE = 0x1F23,   // new in C3 (name ours)
};

class NCAudioOptionWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCAudioOptionWnd, NCWnd, 0, 0x102ED760)

    NCComboBox* SoundCombo;                // +0xE8 level 0 (1.0) .. 5 (Off)
    NCComboBox* MusicCombo;                // +0xEC
    NCComboBox* WavVoiceCombo;             // +0xF0 new in C3 (name ours): SetWavVoiceVolume, id 0x1F22
    NCComboBox* OggVoiceCombo;             // +0xF4 new in C3 (name ours): SetOggVoiceVolume, id 0x1F23
    NCWnd* Unused9C[3];                    // +0xF8 destroyed, never created

    NCAudioOptionWnd();                                              // 0x100cb380
    // ~NCAudioOptionWnd                                             // 0x100a2b40 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (folded)
    virtual INT OnCreate();                                          // 37 0x100c9530
    virtual INT OnDestroy();                                         // 38 0x100c7170
    virtual void ShowWindow();                                       // 48 0x100c4940
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100c6f40

    // names ours
    void ClearItems();                                               // 0x100c4240 (C2 only; before InitVolumes)
    void InitVolumes();                                              // 0x100c47b0
    void SelectVolumes();                                            // 0x100c4270 current levels
    void Apply();                                                    // 0x100c7320
};
NW_ASSERT_OFFSET(NCAudioOptionWnd, SoundCombo, 0xE8);
NW_ASSERT_OFFSET(NCAudioOptionWnd, WavVoiceCombo, 0xF0);
NW_ASSERT_OFFSET(NCAudioOptionWnd, Unused9C, 0xF8);
NW_ASSERT_SIZE(NCAudioOptionWnd, 0x104);

#pragma pack(pop)