#ifndef CYCLONE_ENTITIES_H
#define CYCLONE_ENTITIES_H

#include <c64/types.h>

#define NUM_CRATES      5
#define NUM_SURVIVORS   10
#define NUM_ENTITIES    (NUM_CRATES + NUM_SURVIVORS)

#define ENT_CRATE       0
#define ENT_SURVIVOR    1

#define GAME_PLAYING    0
#define GAME_WIN        1
#define GAME_OVER_LIVES 2
#define GAME_OVER_TIME  3

// Event codes (also consumed by Phase 5 SID audio engine!)
#define EVT_NONE        0
#define EVT_WINCH_START 1
#define EVT_CRATE_GET   2
#define EVT_SURV_GET    3
#define EVT_REFUEL_TICK 4
#define EVT_LOW_FUEL    5
#define EVT_CRASH       6
#define EVT_WIN         7
#define EVT_GAME_OVER   8
#define EVT_AIRCRAFT    9
#define EVT_THUNDER     10

typedef struct RescueEntity {
    int  px;
    int  py;
    byte ground_alt;
    byte kind;       // ENT_CRATE or ENT_SURVIVOR
    byte island_id;
    byte active;     // 1 = waiting on island, 0 = rescued/collected
} RescueEntity;

extern RescueEntity entities[NUM_ENTITIES];
extern byte game_state;
extern byte game_fuel;           // 0..255
extern unsigned int game_time_s; // Mission countdown seconds (480 = 08:00)
extern unsigned int game_score10;// Score / 10 (displays 6 digits up to 655350)
extern byte crates_collected;    // 0..5
extern byte survivors_saved;     // 0..10
extern byte winch_active;        // 1 while hoisting a target
extern byte banner_timer;        // Frames remaining for temporary HUD status banner
extern const char *banner_text;
extern byte banner_color;
extern byte last_sfx_event;      // Latest audio event for SID driver

// Initialize a fresh rescue mission (places 5 crates + 10 survivors on verified flat land)
void entities_init(byte seed);

// Per-frame update: winch hoisting, fuel drain/refuel, mission timer, Base drop-off check
// Returns hardware sprite enable bits for Sprites 3, 6, 7 (caller ORs with heli/hazard masks)
byte entities_update_and_draw(byte south_view, byte is_ntsc);

// Show a temporary cockpit status banner (e.g., "CRATE HOISTED!", "REFUELING...")
void entities_set_banner(const char *msg, byte color, byte frames);

#endif
