#ifndef CYCLONE_AUDIO_H
#define CYCLONE_AUDIO_H

#include <c64/types.h>

#define AUDIO_MODE_SILENT  0
#define AUDIO_MODE_TITLE   1
#define AUDIO_MODE_GAME    2
#define AUDIO_MODE_RADAR   3
#define AUDIO_MODE_WIN     4

// Detected SID chip revision (1 = MOS 8580, 0 = MOS 6581)
extern byte audio_is_8580;

// Mute toggle (0 = audio on, 1 = muted)
extern byte audio_muted;

// Detect SID 6581 vs 8580 and initialize clean audio state
void audio_init(byte is_ntsc);

// Switch active audio mode (cleanly gates off & resets voice envelopes)
void audio_set_mode(byte mode);

// Trigger an immediate one-shot SFX event (EVT_* from entities.h)
void audio_trigger_sfx(byte evt);

// Per-frame tick during Title Screen (plays 3-voice heroic Cyclone SID theme)
void audio_tick_title(void);

// Per-frame tick during live 3D Flight (Rotor V1 + Storm Wind/Thunder V2 + SFX V3)
void audio_tick_game(byte heli_state, byte heli_speed, byte wind_force, byte sfx_evt);

// Per-frame tick during Tactical Radar Map (Sonar Ping + distant storm hush)
void audio_tick_radar(byte wind_force);

#endif
