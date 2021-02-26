//
// Created by dbrent on 2/20/21.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdbool.h>

typedef struct window {
    unsigned int rows;
    unsigned int cols;
    unsigned int prev_rows;
    unsigned int prev_cols;
} window_t;

typedef struct state {
    bool should_close;
    window_t window;
    float *last_line;
} state_t;

extern state_t state;

#endif // GLOBALS_H