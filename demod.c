//
// Created by dbrent on 3/1/21.
//

#include "demod.h"
#include "convenience.h"

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// multiple of these, eventually
struct dongle_state dongle;
struct demod_state demod;
struct output_state output;
struct controller_state controller;

static int *atan_lut = NULL;
static int atan_lut_size = 131072; /* 512 KB */
static int atan_lut_coef = 8;
static int printLevels = 0;
static int printLevelNo = 1;
static int levelMax = 0;
static int levelMaxMax = 0;
static double levelSum = 0.0;

#define safe_cond_signal(n, m)                                                 \
    pthread_mutex_lock(m);                                                     \
    pthread_cond_signal(n);                                                    \
    pthread_mutex_unlock(m)
#define safe_cond_wait(n, m)                                                   \
    pthread_mutex_lock(m);                                                     \
    pthread_cond_wait(n, m);                                                   \
    pthread_mutex_unlock(m)

/* {length, coef, coef, coef}  and scaled by 2^15
   for now, only length 9, optimal way to get +85% bandwidth */
#define CIC_TABLE_MAX 10
int cic_9_tables[][10] = {
        {
                0,
        },
        {9, -156, -97, 2798, -15489, 61019, -15489, 2798, -97, -156},
        {9, -128, -568, 5593, -24125, 74126, -24125, 5593, -568, -128},
        {9, -129, -639, 6187, -26281, 77511, -26281, 6187, -639, -129},
        {9, -122, -612, 6082, -26353, 77818, -26353, 6082, -612, -122},
        {9, -120, -602, 6015, -26269, 77757, -26269, 6015, -602, -120},
        {9, -120, -582, 5951, -26128, 77542, -26128, 5951, -582, -120},
        {9, -119, -580, 5931, -26094, 77505, -26094, 5931, -580, -119},
        {9, -119, -578, 5921, -26077, 77484, -26077, 5921, -578, -119},
        {9, -119, -577, 5917, -26067, 77473, -26067, 5917, -577, -119},
        {9, -199, -362, 5303, -25505, 77489, -25505, 5303, -362, -199},
};

void rotate16_90(int16_t *buf, uint32_t len)
/* 90 rotation is 1+0j, 0+1j, -1+0j, 0-1j
   or [0, 1, -3, 2, -4, -5, 7, -6] */
{
    uint32_t i;
    int16_t tmp;
    for (i = 0; i < len; i += 8) {
        tmp = -buf[i + 3];
        buf[i + 3] = buf[i + 2];
        buf[i + 2] = tmp;

        buf[i + 4] = -buf[i + 4];
        buf[i + 5] = -buf[i + 5];

        tmp = -buf[i + 6];
        buf[i + 6] = buf[i + 7];
        buf[i + 7] = tmp;
    }
}

void rotate_90(unsigned char *buf, uint32_t len)
/* 90 rotation is 1+0j, 0+1j, -1+0j, 0-1j
   or [0, 1, -3, 2, -4, -5, 7, -6] */
{
    uint32_t i;
    unsigned char tmp;
    for (i = 0; i < len; i += 8) {
        /* uint8_t negation = 255 - x */
        tmp = 255 - buf[i + 3];
        buf[i + 3] = buf[i + 2];
        buf[i + 2] = tmp;

        buf[i + 4] = 255 - buf[i + 4];
        buf[i + 5] = 255 - buf[i + 5];

        tmp = 255 - buf[i + 6];
        buf[i + 6] = buf[i + 7];
        buf[i + 7] = tmp;
    }
}

void low_pass(struct demod_state *d)
/* simple square window FIR */
{
    int i = 0, i2 = 0;
    while (i < d->lp_len) {
        d->now_r += d->lowpassed[i];
        d->now_j += d->lowpassed[i + 1];
        i += 2;
        d->prev_index++;
        if (d->prev_index < d->downsample) {
            continue;
        }
        d->lowpassed[i2] = d->now_r;     // * d->output_scale;
        d->lowpassed[i2 + 1] = d->now_j; // * d->output_scale;
        d->prev_index = 0;
        d->now_r = 0;
        d->now_j = 0;
        i2 += 2;
    }
    d->lp_len = i2;
}

int low_pass_simple(int16_t *signal2, int len, int step)
// no wrap around, length must be multiple of step
{
    int i, i2, sum;
    for (i = 0; i < len; i += step) {
        sum = 0;
        for (i2 = 0; i2 < step; i2++) {
            sum += (int)signal2[i + i2];
        }
        // signal2[i/step] = (int16_t)(sum / step);
        signal2[i / step] = (int16_t)(sum);
    }
    signal2[i / step + 1] = signal2[i / step];
    return len / step;
}

void low_pass_real(struct demod_state *s)
/* simple square window FIR */
// add support for upsampling?
{
    int i = 0, i2 = 0;
    int fast = (int)s->rate_out;
    int slow = s->rate_out2;
    while (i < s->result_len) {
        s->now_lpr += s->result[i];
        i++;
        s->prev_lpr_index += slow;
        if (s->prev_lpr_index < fast) {
            continue;
        }
        s->result[i2] = (int16_t)(s->now_lpr / (fast / slow));
        s->prev_lpr_index -= fast;
        s->now_lpr = 0;
        i2 += 1;
    }
    s->result_len = i2;
}

void fifth_order(int16_t *data, int length, int16_t *hist)
/* for half of interleaved data */
{
    int i;
    int16_t a, b, c, d, e, f;
    a = hist[1];
    b = hist[2];
    c = hist[3];
    d = hist[4];
    e = hist[5];
    f = data[0];
    /* a downsample should improve resolution, so don't fully shift */
    data[0] = (a + (b + e) * 5 + (c + d) * 10 + f) >> 4;
    for (i = 4; i < length; i += 4) {
        a = c;
        b = d;
        c = e;
        d = f;
        e = data[i - 2];
        f = data[i];
        data[i / 2] = (a + (b + e) * 5 + (c + d) * 10 + f) >> 4;
    }
    /* archive */
    hist[0] = a;
    hist[1] = b;
    hist[2] = c;
    hist[3] = d;
    hist[4] = e;
    hist[5] = f;
}

void generic_fir(int16_t *data, int length, int *fir, int16_t *hist)
/* Okay, not at all generic.  Assumes length 9, fix that eventually. */
{
    int d, temp, sum;
    for (d = 0; d < length; d += 2) {
        temp = data[d];
        sum = 0;
        sum += (hist[0] + hist[8]) * fir[1];
        sum += (hist[1] + hist[7]) * fir[2];
        sum += (hist[2] + hist[6]) * fir[3];
        sum += (hist[3] + hist[5]) * fir[4];
        sum += hist[4] * fir[5];
        data[d] = sum >> 15;
        hist[0] = hist[1];
        hist[1] = hist[2];
        hist[2] = hist[3];
        hist[3] = hist[4];
        hist[4] = hist[5];
        hist[5] = hist[6];
        hist[6] = hist[7];
        hist[7] = hist[8];
        hist[8] = temp;
    }
}

/* define our own complex math ops
   because ARMv5 has no hardware float */

void multiply(int ar, int aj, int br, int bj, int *cr, int *cj) {
    *cr = ar * br - aj * bj;
    *cj = aj * br + ar * bj;
}

int polar_discriminant(int ar, int aj, int br, int bj) {
    int cr, cj;
    double angle;
    multiply(ar, aj, br, -bj, &cr, &cj);
    angle = atan2((double)cj, (double)cr);
    return (int)(angle / 3.14159 * (1 << 14));
}

int fast_atan2(int y, int x)
/* pre scaled for int16 */
{
    int yabs, angle;
    int pi4 = (1 << 12), pi34 = 3 * (1 << 12); // note pi = 1<<14
    if (x == 0 && y == 0) {
        return 0;
    }
    yabs = y;
    if (yabs < 0) {
        yabs = -yabs;
    }
    if (x >= 0) {
        angle = pi4 - pi4 * (x - yabs) / (x + yabs);
    } else {
        angle = pi34 - pi4 * (x + yabs) / (yabs - x);
    }
    if (y < 0) {
        return -angle;
    }
    return angle;
}

int polar_disc_fast(int ar, int aj, int br, int bj) {
    int cr, cj;
    multiply(ar, aj, br, -bj, &cr, &cj);
    return fast_atan2(cj, cr);
}

int atan_lut_init(void) {
    int i = 0;

    atan_lut = malloc(atan_lut_size * sizeof(int));

    for (i = 0; i < atan_lut_size; i++) {
        atan_lut[i] = (int)(atan((double)i / (1 << atan_lut_coef)) / 3.14159 *
                            (1 << 14));
    }

    return 0;
}

int polar_disc_lut(int ar, int aj, int br, int bj) {
    int cr, cj, x, x_abs;

    multiply(ar, aj, br, -bj, &cr, &cj);

    /* special cases */
    if (cr == 0 || cj == 0) {
        if (cr == 0 && cj == 0) {
            return 0;
        }
        if (cr == 0 && cj > 0) {
            return 1 << 13;
        }
        if (cr == 0 && cj < 0) {
            return -(1 << 13);
        }
        if (cj == 0 && cr > 0) {
            return 0;
        }
        if (cj == 0 && cr < 0) {
            return 1 << 14;
        }
    }

    /* real range -32768 - 32768 use 64x range -> absolute maximum: 2097152 */
    x = (cj << atan_lut_coef) / cr;
    x_abs = abs(x);

    if (x_abs >= atan_lut_size) {
        /* we can use linear range, but it is not necessary */
        return (cj > 0) ? 1 << 13 : -(1 << 13);
    }

    if (x > 0) {
        return (cj > 0) ? atan_lut[x] : atan_lut[x] - (1 << 14);
    } else {
        return (cj > 0) ? (1 << 14) - atan_lut[-x] : -atan_lut[-x];
    }

    return 0;
}

int esbensen(int ar, int aj, int br, int bj)
/*
  input signal: s(t) = a*exp(-i*w*t+p)
  a = amplitude, w = angular freq, p = phase difference
  solve w
  s' = -i(w)*a*exp(-i*w*t+p)
  s'*conj(s) = -i*w*a*a
  s'*conj(s) / |s|^2 = -i*w
*/
{
    int cj, dr, dj;
    int scaled_pi = 2608; /* 1<<14 / (2*pi) */
    dr = (br - ar) * 2;
    dj = (bj - aj) * 2;
    cj = bj * dr - br * dj; /* imag(ds*conj(s)) */
    return (scaled_pi * cj / (ar * ar + aj * aj + 1));
}

void fm_demod(struct demod_state *fm) {
    int i, pcm;
    int16_t *lp = fm->lowpassed;
    pcm = polar_discriminant(lp[0], lp[1], fm->pre_r, fm->pre_j);
    fm->result[0] = (int16_t)pcm;
    for (i = 2; i < (fm->lp_len - 1); i += 2) {
        switch (fm->custom_atan) {
        case 0:
            pcm = polar_discriminant(lp[i], lp[i + 1], lp[i - 2], lp[i - 1]);
            break;
        case 1:
            pcm = polar_disc_fast(lp[i], lp[i + 1], lp[i - 2], lp[i - 1]);
            break;
        case 2:
            pcm = polar_disc_lut(lp[i], lp[i + 1], lp[i - 2], lp[i - 1]);
            break;
        case 3:
            pcm = esbensen(lp[i], lp[i + 1], lp[i - 2], lp[i - 1]);
            break;
        }
        fm->result[i / 2] = (int16_t)pcm;
    }
    fm->pre_r = lp[fm->lp_len - 2];
    fm->pre_j = lp[fm->lp_len - 1];
    fm->result_len = fm->lp_len / 2;
}

void am_demod(struct demod_state *fm)
// todo, fix this extreme laziness
{
    int i, pcm;
    int16_t *lp = fm->lowpassed;
    int16_t *r = fm->result;
    for (i = 0; i < fm->lp_len; i += 2) {
        // hypot uses floats but won't overflow
        // r[i/2] = (int16_t)hypot(lp[i], lp[i+1]);
        pcm = lp[i] * lp[i];
        pcm += lp[i + 1] * lp[i + 1];
        r[i / 2] = (int16_t)sqrt(pcm) * fm->output_scale;
    }
    fm->result_len = fm->lp_len / 2;
    // lowpass? (3khz)  highpass?  (dc)
}

void usb_demod(struct demod_state *fm) {
    int i, pcm;
    int16_t *lp = fm->lowpassed;
    int16_t *r = fm->result;
    for (i = 0; i < fm->lp_len; i += 2) {
        pcm = lp[i] + lp[i + 1];
        r[i / 2] = (int16_t)pcm * fm->output_scale;
    }
    fm->result_len = fm->lp_len / 2;
}

void lsb_demod(struct demod_state *fm) {
    int i, pcm;
    int16_t *lp = fm->lowpassed;
    int16_t *r = fm->result;
    for (i = 0; i < fm->lp_len; i += 2) {
        pcm = lp[i] - lp[i + 1];
        r[i / 2] = (int16_t)pcm * fm->output_scale;
    }
    fm->result_len = fm->lp_len / 2;
}

void raw_demod(struct demod_state *fm) {
    int i;
    for (i = 0; i < fm->lp_len; i++) {
        fm->result[i] = (int16_t)fm->lowpassed[i];
    }
    fm->result_len = fm->lp_len;
}

void deemph_filter(struct demod_state *fm) {
    static int avg; // cheating...
    int i, d;
    // de-emph IIR
    // avg = avg * (1 - alpha) + sample * alpha;
    for (i = 0; i < fm->result_len; i++) {
        d = fm->result[i] - avg;
        if (d > 0) {
            avg += (d + fm->deemph_a / 2) / fm->deemph_a;
        } else {
            avg += (d - fm->deemph_a / 2) / fm->deemph_a;
        }
        fm->result[i] = (int16_t)avg;
    }
}

void dc_block_audio_filter(struct demod_state *fm) {
    int i, avg;
    int64_t sum = 0;
    for (i = 0; i < fm->result_len; i++) {
        sum += fm->result[i];
    }
    avg = sum / fm->result_len;
    avg = (avg + fm->dc_avg * fm->adc_block_const) / (fm->adc_block_const + 1);
    for (i = 0; i < fm->result_len; i++) {
        fm->result[i] -= avg;
    }
    fm->dc_avg = avg;
}

void dc_block_raw_filter(struct demod_state *fm, int16_t *buf, int len) {
    /* derived from dc_block_audio_filter,
            running over the raw I/Q components
    */
    int i, avgI, avgQ;
    int64_t sumI = 0;
    int64_t sumQ = 0;
    for (i = 0; i < len; i += 2) {
        sumI += buf[i];
        sumQ += buf[i + 1];
    }
    avgI = sumI / (len / 2);
    avgQ = sumQ / (len / 2);
    avgI = (avgI + fm->dc_avgI * fm->rdc_block_const) /
           (fm->rdc_block_const + 1);
    avgQ = (avgQ + fm->dc_avgQ * fm->rdc_block_const) /
           (fm->rdc_block_const + 1);
    for (i = 0; i < len; i += 2) {
        buf[i] -= avgI;
        buf[i + 1] -= avgQ;
    }
    fm->dc_avgI = avgI;
    fm->dc_avgQ = avgQ;
}
int mad(int16_t *samples, int len, int step)
/* mean average deviation */
{
    int i = 0, sum = 0, ave = 0;
    if (len == 0) {
        return 0;
    }
    for (i = 0; i < len; i += step) {
        sum += samples[i];
    }
    ave = sum / (len * step);
    sum = 0;
    for (i = 0; i < len; i += step) {
        sum += abs(samples[i] - ave);
    }
    return sum / (len / step);
}

int rms(int16_t *samples, int len, int step)
/* largely lifted from rtl_power */
{
    int i;
    long p, t, s;
    double dc, err;

    p = t = 0L;
    for (i = 0; i < len; i += step) {
        s = (long)samples[i];
        t += s;
        p += s * s;
    }
    /* correct for dc offset in squares */
    dc = (double)(t * step) / (double)len;
    err = t * 2 * dc - dc * dc * len;

    return (int)sqrt((p - err) / len);
}

void full_demod(struct demod_state *d) {
    int i, ds_p;
    int sr = 0;
    ds_p = d->downsample_passes;
    if (ds_p) {
        for (i = 0; i < ds_p; i++) {
            fifth_order(d->lowpassed, (d->lp_len >> i), d->lp_i_hist[i]);
            fifth_order(d->lowpassed + 1, (d->lp_len >> i) - 1,
                        d->lp_q_hist[i]);
        }
        d->lp_len = d->lp_len >> ds_p;
        /* droop compensation */
        if (d->comp_fir_size == 9 && ds_p <= CIC_TABLE_MAX) {
            generic_fir(d->lowpassed, d->lp_len, cic_9_tables[ds_p],
                        d->droop_i_hist);
            generic_fir(d->lowpassed + 1, d->lp_len - 1, cic_9_tables[ds_p],
                        d->droop_q_hist);
        }
    } else {
        low_pass(d);
    }
    /* power squelch */
    if (d->squelch_level) {
        sr = rms(d->lowpassed, d->lp_len, 1);
        if (sr < d->squelch_level) {
            d->squelch_hits++;
            for (i = 0; i < d->lp_len; i++) {
                d->lowpassed[i] = 0;
            }
        } else {
            d->squelch_hits = 0;
        }
    }

    if (printLevels) {
        if (!sr)
            sr = rms(d->lowpassed, d->lp_len, 1);
        --printLevelNo;
        if (printLevels) {
            levelSum += sr;
            if (levelMax < sr)
                levelMax = sr;
            if (levelMaxMax < sr)
                levelMaxMax = sr;
            if (!printLevelNo) {
                printLevelNo = printLevels;
                fprintf(stderr, "%f, %d, %d, %d\n", (levelSum / printLevels),
                        levelMax, levelMaxMax, d->squelch_level);
                levelMax = 0;
                levelSum = 0;
            }
        }
    }
    d->mode_demod(d); /* lowpassed -> result */
    if (d->mode_demod == &raw_demod) {
        return;
    }
    /* todo, fm noise squelch */
    // use nicer filter here too?
    if (d->post_downsample > 1) {
        d->result_len =
                low_pass_simple(d->result, d->result_len, d->post_downsample);
    }
    if (d->deemph) {
        deemph_filter(d);
    }
    if (d->dc_block_audio) {
        dc_block_audio_filter(d);
    }
    if (d->rate_out2 > 0) {
        low_pass_real(d);
        // arbitrary_resample(d->result, d->result, d->result_len, d->result_len
        // * d->rate_out2 / d->rate_out);
    }
}

void dongle_init(struct dongle_state *s) {
    s->rate = DEFAULT_SAMPLE_RATE;
    s->gain_str = NULL;
    s->mute = 0;
    s->direct_sampling = 0;
    s->offset_tuning = 0;
    s->demod_target = &demod;
    s->bandwidth = 0;
    s->channel = 0;
}

void demod_init(struct demod_state *s) {
    s->rate_in = DEFAULT_SAMPLE_RATE;
    s->rate_out = DEFAULT_SAMPLE_RATE;
    s->squelch_level = 0;
    s->conseq_squelch = 10;
    s->terminate_on_squelch = 0;
    s->squelch_hits = 11;
    s->downsample_passes = 0;
    s->comp_fir_size = 0;
    s->prev_index = 0;
    s->post_downsample = 1; // once this works, default = 4
    s->custom_atan = 0;
    s->deemph = 0;
    s->rate_out2 = -1; // flag for disabled
    s->mode_demod = &fm_demod;
    s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
    s->prev_lpr_index = 0;
    s->deemph_a = 0;
    s->now_lpr = 0;
    s->dc_block_audio = 0;
    s->dc_avg = 0;
    s->adc_block_const = 9;
    s->dc_block_raw = 0;
    s->dc_avgI = 0;
    s->dc_avgQ = 0;
    s->rdc_block_const = 9;
    pthread_rwlock_init(&s->rw, NULL);
    pthread_cond_init(&s->ready, NULL);
    pthread_mutex_init(&s->ready_m, NULL);
    s->output_target = &output;
}

void demod_cleanup(struct demod_state *s) {
    pthread_rwlock_destroy(&s->rw);
    pthread_cond_destroy(&s->ready);
    pthread_mutex_destroy(&s->ready_m);
}

void output_init(struct output_state *s) {
    // s->rate = DEFAULT_SAMPLE_RATE;
    pthread_rwlock_init(&s->rw, NULL);
    pthread_cond_init(&s->ready, NULL);
    pthread_mutex_init(&s->ready_m, NULL);
}

void output_cleanup(struct output_state *s) {
    pthread_rwlock_destroy(&s->rw);
    pthread_cond_destroy(&s->ready);
    pthread_mutex_destroy(&s->ready_m);
}

void controller_init(struct controller_state *s) {
    s->freqs[0] = 100000000;
    s->freq_len = 0;
    s->edge = 0;
    s->wb_mode = 0;
    pthread_cond_init(&s->hop, NULL);
    pthread_mutex_init(&s->hop_m, NULL);
}

void controller_cleanup(struct controller_state *s) {
    pthread_cond_destroy(&s->hop);
    pthread_mutex_destroy(&s->hop_m);
}