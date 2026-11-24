#ifndef CYCLONE_TITLE_H
#define CYCLONE_TITLE_H

#include <c64/types.h>

// Persistent high score across missions (stored in units of 10 pts)
extern unsigned int title_high_score10;

// Update high score if current mission score beats it
void title_record_score(unsigned int score10);

// Show animated Title Screen & Options Menu until player launches mission
void title_screen_show(byte is_ntsc);

// Show full-screen Controls & Mission Help screen (works from both Title Menu and live Flight!)
void help_screen_show(byte is_ntsc, byte in_flight);

#endif
