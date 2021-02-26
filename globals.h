//
// Created by dbrent on 2/20/21.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdbool.h>

typedef struct state {
    bool should_close;
    float *last_line;
} state_t;

extern state_t state;

#endif // GLOBALS_H