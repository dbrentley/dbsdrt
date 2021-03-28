//
// Created by dbrent on 2/20/21.
//

#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int8_t i;
    int8_t q;
} iq_pair_t;

typedef struct {
    int8_t *sample;
    size_t size;

} sample_data_t;

typedef struct {
    int rows;
    int cols;
    int prev_rows;
    int prev_cols;
} window_t;

typedef struct {
    bool volatile should_close;
    window_t window;
    float *last_line;
} state_t;

extern state_t state;

#endif // GLOBALS_H