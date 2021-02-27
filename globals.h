//
// Created by dbrent on 2/20/21.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdbool.h>

typedef struct window {
    int rows;
    int cols;
    int prev_rows;
    int prev_cols;
} window_t;

typedef struct state {
    bool should_close;
    window_t window;
    float *last_line;
} state_t;

extern state_t state;

#endif // GLOBALS_H