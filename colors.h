
#ifndef __COLORS_H__
#define __COLORS_H__

#include <pthreadGridVisi.h>

/* This file defines some basic colors using RGB values. */

color3 c3_black = { 0, 0, 0 };
color3 c3_red = { 255, 0, 0 };
color3 c3_orange = { 255, 128, 0 };
color3 c3_yellow = { 255, 255, 0 };
color3 c3_green = { 0, 255, 0 };
color3 c3_blue = { 0, 0, 255 };
color3 c3_purple = { 128, 0, 255 };
color3 c3_white = { 255, 255, 255 };
color3 c3_pink = { 255, 0, 128 };
color3 c3_teal = { 50, 255, 255 };
color3 c3_brown = { 100, 50, 0 };

/* You can use this array to assign colors to threads based on the thread id
 * number you give them. */

color3 colors[8] = {
    // Red
    { 255, 0, 0 },
    // Pink
    { 255, 0, 128 },
    // Orange
    { 255, 128, 0 },
    // Yellow
    { 255, 255, 0 },
    // Green
    { 0, 255, 0 },
    // Teal
    { 50, 255, 255 },
    // Blue
    { 0, 0, 255 },
    // Purple
    { 128, 0, 255 }
};


#endif  /* __COLORS_H__ */
