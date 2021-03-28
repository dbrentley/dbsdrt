//
// Created by dbrent on 3/1/21.
//

#ifndef DBSDRT_DEMOD_H
#define DBSDRT_DEMOD_H

#include "convenience.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define DEFAULT_SAMPLE_RATE 24000
#define DEFAULT_BUF_LENGTH (1 * 16384)
#define MAXIMUM_OVERSAMPLE 16
#define MAXIMUM_BUF_LENGTH (MAXIMUM_OVERSAMPLE * DEFAULT_BUF_LENGTH)
#define BUFFER_DUMP 4096
#define FREQUENCIES_LIMIT 1000

struct dongle_state {
    int exit_flag;
    pthread_t thread;
    SoapySDRDevice *dev;
    SoapySDRStream *stream;
    size_t channel;
    char *dev_query;
    uint32_t freq;
    uint32_t rate;
    uint32_t bandwidth;
    char *gain_str;
    int16_t buf16[MAXIMUM_BUF_LENGTH];
    int ppm_error;
    int offset_tuning;
    int direct_sampling;
    int mute;
    struct demod_state *demod_target;
};

struct demod_state {
    int exit_flag;
    pthread_t thread;
    int16_t lowpassed[MAXIMUM_BUF_LENGTH];
    int lp_len;
    int16_t lp_i_hist[10][6];
    int16_t lp_q_hist[10][6];
    int16_t result[MAXIMUM_BUF_LENGTH];
    int16_t droop_i_hist[9];
    int16_t droop_q_hist[9];
    int result_len;
    int rate_in;
    int rate_out;
    int rate_out2;
    int now_r, now_j;
    int pre_r, pre_j;
    int prev_index;
    int downsample; /* min 1, max 256 */
    int post_downsample;
    int output_scale;
    int squelch_level, conseq_squelch, squelch_hits, terminate_on_squelch,
            squelch_zero;
    int downsample_passes;
    int comp_fir_size;
    int custom_atan;
    int deemph, deemph_a;
    int now_lpr;
    int prev_lpr_index;
    int dc_block_audio, dc_avg, adc_block_const;
    int dc_block_raw, dc_avgI, dc_avgQ, rdc_block_const;
    void (*mode_demod)(struct demod_state *);
    pthread_rwlock_t rw;
    pthread_cond_t ready;
    pthread_mutex_t ready_m;
    struct output_state *output_target;
};

struct output_state {
    int exit_flag;
    pthread_t thread;
    FILE *file;
    char *filename;
    int16_t result[MAXIMUM_BUF_LENGTH];
    int result_len;
    int rate;
    int wav_format;
    pthread_rwlock_t rw;
    pthread_cond_t ready;
    pthread_mutex_t ready_m;
};

struct controller_state {
    int exit_flag;
    pthread_t thread;
    uint32_t freqs[FREQUENCIES_LIMIT];
    int freq_len;
    int freq_now;
    int edge;
    int wb_mode;
    pthread_cond_t hop;
    pthread_mutex_t hop_m;
};

struct dongle_state dongle;
struct demod_state demod;
struct output_state output;
struct controller_state controller;

void dc_block_raw_filter(struct demod_state *fm, int16_t *buf, int len);

void dongle_init(struct dongle_state *s);

void demod_init(struct demod_state *s);

void demod_cleanup(struct demod_state *s);

void output_init(struct output_state *s);

void output_cleanup(struct output_state *s);

void controller_init(struct controller_state *s);

void controller_cleanup(struct controller_state *s);

#endif // DBSDRT_DEMOD_H
