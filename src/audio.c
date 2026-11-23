#include "memmap.h"
#include "entities.h"
#include "helicopter.h"
#include "audio.h"

#include <c64/sid.h>
#include <c64/vic.h>

byte audio_is_8580 = 1;
byte audio_muted   = 0;

static byte audio_mode    = AUDIO_MODE_SILENT;
static byte audio_ntsc    = 0;
static byte ntsc_skip     = 0;
static byte audio_tick    = 0;

// In-game synth state
static byte rotor_tmr     = 0;
static byte rotor_pulse   = 0;
static byte thunder_tmr   = 0;
static byte sfx_active    = 0;
static byte sfx_step      = 0;
static byte sfx_tmr       = 0;
static byte radar_tmr     = 0;

// Title theme sequencer state
static byte song_step     = 0;
static byte song_div      = 0;
static unsigned int lead_freq = 0;

// 11-bit SID filter cutoff helper ($D415 bits 0..2, $D416 bits 3..10)
static void sid_set_cutoff(unsigned int fc) {
    if (fc > 2047) fc = 2047;
    ((volatile byte *)0xd415)[0] = (byte)(fc & 7);
    ((volatile byte *)0xd416)[0] = (byte)(fc >> 3);
}

// Calibrated cutoff mapping so 6581 (higher floor) & 8580 (darker linear) both sound rich
static unsigned int cal_cutoff(unsigned int base_8580) {
    if (audio_is_8580) return base_8580;
    // 6581 needs higher cutoff floor + gentler top to avoid harshness
    unsigned int c = 320 + ((base_8580 * 3) >> 2);
    return (c > 2000) ? 2000 : c;
}

// Classic SID 6581 vs 8580 detection via Voice 3 combined waveform (Saw+Tri) readback
static byte detect_sid_8580(void) {
    // Silence all voices first
    for (byte i = 0; i < 25; i++) {
        ((volatile byte *)0xd400)[i] = 0;
    }
    sid.voices[2].freq = 0xffff;
    sid.voices[2].ctrl = SID_CTRL_TEST;
    // Wait a couple of raster lines for oscillator reset
    byte r0 = vic.raster;
    while (vic.raster == r0) {}
    sid.voices[2].ctrl = SID_CTRL_SAW | SID_CTRL_TRI; // Combined waveform (no gate)
    r0 = vic.raster;
    while (vic.raster == r0) {}
    r0 = vic.raster;
    while (vic.raster == r0) {}
    byte sample = sid.random;
    sid.voices[2].ctrl = 0;
    // On MOS 8580 combined waveform returns >= 2; on 6581 analog pull-down yields 0 or 1
    return (sample >= 2) ? 1 : 0;
}

static void sid_silence_all(void) {
    sid.voices[0].ctrl = 0;
    sid.voices[1].ctrl = 0;
    sid.voices[2].ctrl = 0;
    sid.voices[0].attdec = 0x00; sid.voices[0].susrel = 0x00;
    sid.voices[1].attdec = 0x00; sid.voices[1].susrel = 0x00;
    sid.voices[2].attdec = 0x00; sid.voices[2].susrel = 0x00;
    sid.resfilt  = 0x00;
    sid.fmodevol = 0x0f;
}

void audio_init(byte is_ntsc) {
    audio_ntsc    = is_ntsc;
    audio_is_8580 = detect_sid_8580();
    audio_muted   = 0;
    ntsc_skip     = 0;
    audio_tick    = 0;
    thunder_tmr   = 0;
    sfx_active    = 0;
    sfx_step      = 0;
    sfx_tmr       = 0;
    song_step     = 0;
    song_div      = 0;
    sid_silence_all();
    audio_mode    = AUDIO_MODE_SILENT;
}

void audio_set_mode(byte mode) {
    audio_mode  = mode;
    sfx_active  = 0;
    sfx_step    = 0;
    sfx_tmr     = 0;
    thunder_tmr = 0;
    rotor_tmr   = 0;
    radar_tmr   = 8;
    song_step   = 0;
    song_div    = 0;
    sid_silence_all();

    if (mode == AUDIO_MODE_GAME) {
        // Voice 1: Helicopter rotor blade thump (filtered noise + pulse)
        sid.voices[0].attdec = SID_ATK_2 | SID_DKY_72;
        sid.voices[0].susrel = 0x42; // Medium sustain, crisp release
        sid.voices[0].pwm    = 0x0600;

        // Voice 2: Continuous Cyclone wind bed (white noise routed through resonant filter!)
        sid.voices[1].freq   = 0x1600;
        sid.voices[1].attdec = SID_ATK_100 | SID_DKY_240;
        sid.voices[1].susrel = 0x88;
        sid.voices[1].ctrl   = SID_CTRL_NOISE | SID_CTRL_GATE;

        // Route Voice 1 & Voice 2 through SID filter; keep Voice 3 raw & punchy for SFX!
        sid.resfilt  = 0x60 | SID_FILTER_1 | SID_FILTER_2;
        sid.fmodevol = SID_FMODE_LP | SID_FMODE_BP | 0x0f;
        sid_set_cutoff(cal_cutoff(420));
    } else if (mode == AUDIO_MODE_RADAR) {
        // Soft distant storm hush on Voice 2 + crisp Sonar Ping on Voice 3
        sid.voices[1].freq   = 0x0d00;
        sid.voices[1].attdec = SID_ATK_250 | SID_DKY_300;
        sid.voices[1].susrel = 0x36;
        sid.voices[1].ctrl   = SID_CTRL_NOISE | SID_CTRL_GATE;
        sid.resfilt  = 0x40 | SID_FILTER_2;
        sid.fmodevol = SID_FMODE_LP | 0x0d;
        sid_set_cutoff(cal_cutoff(220));
    } else if (mode == AUDIO_MODE_TITLE) {
        // 3-Voice Rescue Theme setup: V1 filtered bass, V2 PWM arp, V3 heroic lead
        sid.voices[0].attdec = SID_ATK_2 | SID_DKY_168;
        sid.voices[0].susrel = 0x34;
        sid.voices[0].pwm    = 0x0580;

        sid.voices[1].attdec = SID_ATK_2 | SID_DKY_114;
        sid.voices[1].susrel = 0x24;
        sid.voices[1].pwm    = 0x0400;

        sid.voices[2].attdec = SID_ATK_8 | SID_DKY_240;
        sid.voices[2].susrel = 0x76;
        sid.voices[2].pwm    = 0x0700;

        sid.resfilt  = 0x50 | SID_FILTER_1;
        sid.fmodevol = SID_FMODE_LP | 0x0f;
        sid_set_cutoff(cal_cutoff(750));
    }
}

// ============================================================================
// Priority SFX Table & Sequencer (Voice 3)
// ============================================================================
static byte sfx_priority(byte evt) {
    switch (evt) {
        case EVT_CRASH:       return 9;
        case EVT_WIN:         return 9;
        case EVT_GAME_OVER:   return 9;
        case EVT_CRATE_GET:   return 7;
        case EVT_SURV_GET:    return 6;
        case EVT_AIRCRAFT:    return 5;
        case EVT_WINCH_START: return 4;
        case EVT_LOW_FUEL:    return 3;
        case EVT_REFUEL_TICK: return 2;
        default:              return 0;
    }
}

void audio_trigger_sfx(byte evt) {
    if (evt == EVT_NONE) return;
    if (evt == EVT_THUNDER) {
        // Thunder rides Voice 2 (storm channel) so it never interrupts Voice 3 SFX!
        thunder_tmr = 46;
        return;
    }
    if (sfx_active != EVT_NONE && sfx_priority(evt) < sfx_priority(sfx_active)) {
        return;
    }
    sfx_active = evt;
    sfx_step   = 0;
    sfx_tmr    = 0;
}

// Compact note table (PAL SID frequencies; sounds crisp on NTSC too within 3.8% musical tolerance)
static const unsigned int NOTE_TBL[24] = {
    // 0..11: Octave 3 (C3..B3)
    1125, 1192, 1263, 1338, 1418, 1502, 1591, 1686, 1786, 1892, 2005, 2124,
    // 12..23: Octave 4 (C4..B4)
    2250, 2384, 2526, 2676, 2835, 3004, 3183, 3372, 3573, 3785, 4010, 4249
};

static unsigned int note_oct(byte idx, byte shift_up) {
    return NOTE_TBL[idx] << shift_up;
}

static void step_sfx_voice3(void) {
    if (sfx_active == EVT_NONE) return;

    if (sfx_tmr > 0) {
        sfx_tmr--;
        // Continuous pitch sweeps inside active steps
        if (sfx_active == EVT_AIRCRAFT) {
            // Dramatic Doppler flyby sweep!
            unsigned int f = 6800 - ((unsigned int)sfx_step * 120);
            if (f < 900) f = 900;
            sid.voices[2].freq = f;
            sfx_step++;
            if (sfx_tmr == 0) {
                sid.voices[2].ctrl = SID_CTRL_SAW; // Gate off
                sfx_active = EVT_NONE;
            }
            return;
        }
        if (sfx_active == EVT_CRASH) {
            // Rumbling descending explosion noise!
            unsigned int f = 3600 - ((unsigned int)sfx_step * 58);
            if (f < 260) f = 260;
            sid.voices[2].freq = f;
            sfx_step++;
            if (sfx_tmr == 0) {
                sid.voices[2].ctrl = SID_CTRL_NOISE;
                sfx_active = EVT_NONE;
            }
            return;
        }
        if (sfx_active == EVT_WINCH_START) {
            // Rising servo whirr
            sid.voices[2].freq = 1800 + ((unsigned int)sfx_step * 180);
            sfx_step++;
            if (sfx_tmr == 0) {
                sid.voices[2].ctrl = SID_CTRL_TRI;
                sfx_active = EVT_NONE;
            }
            return;
        }
        return;
    }

    switch (sfx_active) {
        case EVT_CRATE_GET: {
            static const byte arp[5] = { 12, 16, 19, 12, 0xff }; // C5, E5, G5, C6
            byte n = arp[sfx_step];
            if (n == 0xff) {
                sid.voices[2].ctrl = SID_CTRL_RECT;
                sfx_active = EVT_NONE;
            } else {
                sid.voices[2].attdec = SID_ATK_2 | SID_DKY_114;
                sid.voices[2].susrel = 0x55;
                sid.voices[2].pwm    = 0x0600 + ((unsigned int)sfx_step << 8);
                sid.voices[2].freq   = note_oct(n, (sfx_step == 3) ? 2 : 1);
                sid.voices[2].ctrl   = SID_CTRL_RECT | SID_CTRL_GATE;
                sfx_tmr = (sfx_step == 3) ? 14 : 5;
                sfx_step++;
            }
            break;
        }
        case EVT_SURV_GET: {
            static const byte arp[4] = { 19, 23, 14, 0xff }; // G5, B5, D6
            byte n = arp[sfx_step];
            if (n == 0xff) {
                sid.voices[2].ctrl = SID_CTRL_TRI;
                sfx_active = EVT_NONE;
            } else {
                sid.voices[2].attdec = SID_ATK_2 | SID_DKY_114;
                sid.voices[2].susrel = 0x65;
                sid.voices[2].freq   = note_oct(n, (sfx_step == 2) ? 2 : 1);
                sid.voices[2].ctrl   = SID_CTRL_TRI | SID_CTRL_GATE;
                sfx_tmr = (sfx_step == 2) ? 12 : 5;
                sfx_step++;
            }
            break;
        }
        case EVT_WINCH_START:
            sid.voices[2].attdec = SID_ATK_8 | SID_DKY_72;
            sid.voices[2].susrel = 0x94;
            sid.voices[2].freq   = 1800;
            sid.voices[2].ctrl   = SID_CTRL_TRI | SID_CTRL_GATE;
            sfx_tmr = 16;
            break;

        case EVT_REFUEL_TICK:
            if (sfx_step == 0) {
                sid.voices[2].attdec = SID_ATK_2 | SID_DKY_48;
                sid.voices[2].susrel = 0x22;
                sid.voices[2].freq   = 5200;
                sid.voices[2].ctrl   = SID_CTRL_TRI | SID_CTRL_GATE;
                sfx_tmr = 4;
                sfx_step = 1;
            } else {
                sid.voices[2].ctrl = SID_CTRL_TRI;
                sfx_active = EVT_NONE;
            }
            break;

        case EVT_LOW_FUEL:
            if (sfx_step < 2) {
                sid.voices[2].attdec = SID_ATK_2 | SID_DKY_72;
                sid.voices[2].susrel = 0x52;
                sid.voices[2].pwm    = 0x0800;
                sid.voices[2].freq   = (sfx_step == 0) ? 7570 : 5670; // Urgent A5 -> E5 klaxon
                sid.voices[2].ctrl   = SID_CTRL_RECT | SID_CTRL_GATE;
                sfx_tmr = 6;
                sfx_step++;
            } else {
                sid.voices[2].ctrl = SID_CTRL_RECT;
                sfx_active = EVT_NONE;
            }
            break;

        case EVT_AIRCRAFT:
            sid.voices[2].attdec = SID_ATK_24 | SID_DKY_240;
            sid.voices[2].susrel = 0x76;
            sid.voices[2].freq   = 6800;
            sid.voices[2].ctrl   = SID_CTRL_SAW | SID_CTRL_GATE;
            sfx_tmr = 38;
            break;

        case EVT_CRASH:
            sid.voices[2].attdec = SID_ATK_2 | SID_DKY_750;
            sid.voices[2].susrel = 0x49;
            sid.voices[2].freq   = 3600;
            sid.voices[2].ctrl   = SID_CTRL_NOISE | SID_CTRL_GATE;
            sfx_tmr = 48;
            break;

        case EVT_WIN: {
            static const byte fanfare[7] = { 12, 16, 19, 17, 19, 12, 0xff }; // C-E-G-F-G-C6!
            byte n = fanfare[sfx_step];
            if (n == 0xff) {
                sid.voices[2].ctrl = SID_CTRL_RECT;
                sfx_active = EVT_NONE;
            } else {
                sid.voices[2].attdec = SID_ATK_8 | SID_DKY_240;
                sid.voices[2].susrel = 0x87;
                sid.voices[2].pwm    = 0x0680;
                sid.voices[2].freq   = note_oct(n, (sfx_step == 5) ? 2 : 1);
                sid.voices[2].ctrl   = SID_CTRL_RECT | SID_CTRL_GATE;
                sfx_tmr = (sfx_step == 5) ? 28 : 8;
                sfx_step++;
            }
            break;
        }
        case EVT_GAME_OVER: {
            static const byte sad[5] = { 14, 13, 12, 9, 0xff }; // D-C#-C-A descending
            byte n = sad[sfx_step];
            if (n == 0xff) {
                sid.voices[2].ctrl = SID_CTRL_SAW;
                sfx_active = EVT_NONE;
            } else {
                sid.voices[2].attdec = SID_ATK_16 | SID_DKY_300;
                sid.voices[2].susrel = 0x67;
                sid.voices[2].freq   = note_oct(n, 0);
                sid.voices[2].ctrl   = SID_CTRL_SAW | SID_CTRL_GATE;
                sfx_tmr = (sfx_step == 3) ? 28 : 11;
                sfx_step++;
            }
            break;
        }
        default:
            sfx_active = EVT_NONE;
            break;
    }
}

// ============================================================================
// Live 3D Flight Audio Tick (Voice 1 Rotor + Voice 2 Wind/Thunder + Voice 3 SFX)
// ============================================================================
void audio_tick_game(byte heli_state, byte heli_speed, byte wind_force, byte sfx_evt) {
    if (sfx_evt != EVT_NONE) audio_trigger_sfx(sfx_evt);
    if (audio_muted) {
        sid.fmodevol = 0x00;
        return;
    }

    // PAL/NTSC speed normalization: skip 1 of every 6 ticks on 60Hz NTSC
    if (audio_ntsc) {
        ntsc_skip++;
        if (ntsc_skip >= 6) {
            ntsc_skip = 0;
            return;
        }
    }
    audio_tick++;

    // 1. Voice 1: Dynamic Helicopter Rotor Blade-Pass Synthesis
    if (heli_state == HELI_STATE_CRASHING) {
        // Duck rotor during explosion so crash rumble shines
        sid.voices[0].ctrl = SID_CTRL_NOISE;
    } else {
        // Blade-pass interval: 9 frames idle -> 6 hover -> 4 full speed!
        byte period;
        if (heli_state == HELI_STATE_LANDED) {
            period = 9;
        } else if (heli_speed > 26) {
            period = 4;
        } else if (heli_speed > 10) {
            period = 5;
        } else {
            period = 6;
        }

        if (rotor_tmr == 0) {
            rotor_tmr = period;
            rotor_pulse ^= 1;
            // Alternate soft low noise "whup" & tight sub-bass thump for authentic chopper beat!
            unsigned int base_f = (heli_state == HELI_STATE_LANDED)
                                ? 0x0380
                                : (0x04a0 + ((unsigned int)heli_speed << 3));
            sid.voices[0].freq   = base_f;
            sid.voices[0].attdec = SID_ATK_2 | ((heli_state == HELI_STATE_LANDED) ? SID_DKY_48 : SID_DKY_72);
            sid.voices[0].susrel = (heli_state == HELI_STATE_LANDED) ? 0x22 : 0x43;
            sid.voices[0].ctrl   = SID_CTRL_NOISE | SID_CTRL_GATE;
        } else {
            rotor_tmr--;
            if (rotor_tmr == (byte)(period >> 1)) {
                sid.voices[0].ctrl = SID_CTRL_NOISE; // Gate off for rhythmic chop!
            }
        }
    }

    // 2. Voice 2 + Shared SID Filter: Dynamic Cyclone Wind & Booming Thunder!
    if (thunder_tmr > 0) {
        thunder_tmr--;
        // Booming Low-Pass thunder sweep: bright crack -> rolling sub-bass tail
        sid.voices[1].freq   = 0x0900 + ((unsigned int)thunder_tmr << 5);
        sid.voices[1].susrel = 0xc9;
        sid.voices[1].ctrl   = SID_CTRL_NOISE | SID_CTRL_GATE;
        sid.resfilt  = 0x90 | SID_FILTER_1 | SID_FILTER_2;
        sid.fmodevol = SID_FMODE_LP | 0x0f;
        sid_set_cutoff(cal_cutoff(200 + ((unsigned int)thunder_tmr * 28)));
    } else {
        // Continuous howling storm wind modulated by wind_force (1..16) + slow swirling LFO
        byte lfo = (byte)((audio_tick & 31) < 16 ? (audio_tick & 15) : (15 - (audio_tick & 15)));
        unsigned int wf = (unsigned int)wind_force;
        unsigned int fc = 150 + (wf * 48) + (lfo * (wf + 2));
        byte res = (wind_force >= 12) ? 0xb0 : ((wind_force >= 7) ? 0x70 : 0x30);
        byte fmode = (wind_force >= 10) ? (SID_FMODE_BP | SID_FMODE_LP) : SID_FMODE_LP;

        sid.voices[1].freq   = 0x0c00 + (wf << 7);
        sid.voices[1].susrel = (wind_force >= 11) ? 0x98 : ((wind_force >= 6) ? 0x58 : 0x26);
        sid.voices[1].ctrl   = SID_CTRL_NOISE | SID_CTRL_GATE;
        sid.resfilt  = res | SID_FILTER_1 | SID_FILTER_2;
        sid.fmodevol = fmode | 0x0f;
        sid_set_cutoff(cal_cutoff(fc));
    }

    // 3. Voice 3: Priority SFX Sequencer
    step_sfx_voice3();
}

// ============================================================================
// Tactical Radar Sonar Audio Tick
// ============================================================================
void audio_tick_radar(byte wind_force) {
    if (audio_muted) {
        sid.fmodevol = 0x00;
        return;
    }
    if (audio_ntsc) {
        ntsc_skip++;
        if (ntsc_skip >= 6) { ntsc_skip = 0; return; }
    }
    audio_tick++;

    // Soft distant wind hush proportional to storm proximity
    unsigned int fc = 140 + ((unsigned int)wind_force * 20);
    sid_set_cutoff(cal_cutoff(fc));
    sid.fmodevol = SID_FMODE_LP | 0x0d;

    // Crisp atmospheric sonar radar ping every 44 frames (~0.9s)
    if (radar_tmr == 0) {
        radar_tmr = 44;
        sid.voices[2].attdec = SID_ATK_2 | SID_DKY_240;
        sid.voices[2].susrel = 0x08;
        sid.voices[2].freq   = 0x3800; // Clean sonar ping tone
        sid.voices[2].ctrl   = SID_CTRL_TRI | SID_CTRL_GATE;
    } else {
        radar_tmr--;
        if (radar_tmr == 38) {
            sid.voices[2].ctrl = SID_CTRL_TRI; // Smooth release tail
        }
    }
}

// ============================================================================
// Title Screen 3-Voice Original SID Theme ("Cyclone Rescue" in D Minor)
// ============================================================================
// 32-step looping bassline (index into NOTE_TBL, octave 2 -> >> 1)
static const byte SONG_BASS[32] = {
    2, 2, 14, 2,  2, 2, 12, 2,   // Dm driving octave bass
    10, 10, 22, 10, 10, 10, 19, 10, // Bb driving octave bass
    0, 0, 12, 0,  0, 0, 10, 0,   // C driving octave bass
    9, 9, 21, 9,  7, 9, 12, 13   // A7 turnaround -> Dm
};

// 32-step arpeggio chords (index into NOTE_TBL, octave 3)
static const byte SONG_ARP[32] = {
    2, 5, 9, 14,  9, 5, 2, 9,
    10, 14, 17, 22, 17, 14, 10, 17,
    0, 4, 7, 12,  7, 4, 0, 7,
    9, 13, 16, 21, 16, 13, 9, 16
};

// 32-step heroic melody (0xfe = sustain, 0xff = rest, else NOTE_TBL index shifted << 1)
static const byte SONG_LEAD[32] = {
    14, 0xfe, 17, 16, 14, 0xfe, 12, 0xfe,
    10, 0xfe, 14, 17, 16, 0xfe, 14, 0xff,
    12, 0xfe, 16, 19, 17, 0xfe, 16, 14,
    13, 0xfe, 16, 0xfe, 21, 0xfe, 0xff, 0xff
};

void audio_tick_title(void) {
    if (audio_muted) {
        sid.fmodevol = 0x00;
        return;
    }
    if (audio_ntsc) {
        ntsc_skip++;
        if (ntsc_skip >= 6) { ntsc_skip = 0; return; }
    }
    audio_tick++;

    // Subtle filter sweep on Bass & PWM shimmer on Arp + Vibrato on Lead
    byte lfo = (byte)((audio_tick & 31) < 16 ? (audio_tick & 15) : (15 - (audio_tick & 15)));
    sid_set_cutoff(cal_cutoff(520 + ((unsigned int)lfo * 34)));
    sid.voices[1].pwm = 0x0300 + ((unsigned int)lfo << 6);
    if (lead_freq > 0) {
        sbyte vib = (audio_tick & 4) ? 14 : -14;
        sid.voices[2].freq = (unsigned int)((int)lead_freq + vib);
    }

    if (song_div == 0) {
        song_div = 6; // Crisp 125 BPM 8th-note groove
        byte st = song_step;

        // Voice 1: Driving filtered Saw+Pulse Bass
        sid.voices[0].ctrl = SID_CTRL_SAW;
        sid.voices[0].freq = NOTE_TBL[SONG_BASS[st]] >> 1;
        sid.voices[0].ctrl = SID_CTRL_SAW | SID_CTRL_GATE;

        // Voice 2: Sparkling PWM Arpeggio
        sid.voices[1].ctrl = SID_CTRL_RECT;
        sid.voices[1].freq = NOTE_TBL[SONG_ARP[st]];
        sid.voices[1].ctrl = SID_CTRL_RECT | SID_CTRL_GATE;

        // Voice 3: Heroic Rescue Lead
        byte ln = SONG_LEAD[st];
        if (ln == 0xff) {
            sid.voices[2].ctrl = SID_CTRL_RECT;
            lead_freq = 0;
        } else if (ln != 0xfe) {
            sid.voices[2].ctrl = SID_CTRL_RECT;
            lead_freq = NOTE_TBL[ln] << 1;
            sid.voices[2].freq = lead_freq;
            sid.voices[2].ctrl = SID_CTRL_RECT | SID_CTRL_GATE;
        }

        song_step = (byte)((st + 1) & 31);
    } else {
        song_div--;
        if (song_div == 1) {
            // Crisp rhythmic staccato release on Bass & Arp before next 8th note
            sid.voices[0].ctrl = SID_CTRL_SAW;
            sid.voices[1].ctrl = SID_CTRL_RECT;
        }
    }
}
