#include "demod.h"
#include "device.h"
#include "fft.h"
#include "globals.h"
#include "queue.h"
#include "signal_handler.h"

#include <SoapySDR/Formats.h>
#include <complex.h>
#include <limits.h>
#include <math.h>
#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_SAMPLE_RATE 240000
#define DEFAULT_FREQUENCY 860719700 // 858245000
#define DEFAULT_LNA_GAIN 24
#define DEFAULT_VGA_GAIN 24
#define FFT_SIZE 8192              // max BYTES_PER_TRANSFER
#define FREQ_MIN_HZ (0ull)         /* 0 Hz */
#define FREQ_MAX_HZ (7250000000ll) /* 7250MHz */
#define DEFAULT_FM_BANDWIDTH 12000
#define DECIMATE_TRANSITION_BW 800

queue_t mag_line_queue;
state_t state;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int has_init = 0;
int verbosity = 0;
int tmp_stdout = -1;
int lcm_post[17] = {1, 1, 1, 3, 1, 5, 3, 7, 1, 9, 5, 11, 3, 13, 7, 15, 1};
int ACTUAL_BUF_LENGTH;

float hamming(float x) {
    return (float)(0.54 - 0.46 * (2 * M_PI * (0.5 + (x / 2))));
} // hamming window function used for FIR filter design

void receive_callback(void *samples, size_t n_samples,
                      size_t bytes_per_sample) {
    // n_samples: 131072
    // ffts: 16
    // bytes_per_sample: 2
    int8_t *buff = (int8_t *)samples;
    size_t ffts = n_samples / FFT_SIZE;

    // TODO store in a buffer for fft size larger than n samples
    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < ffts; i++) {
        float *line = fft(buff + i * FFT_SIZE * bytes_per_sample);
        queue_append(&mag_line_queue, line);
    }
    pthread_mutex_unlock(&lock);
}

void receive_callback_test(void *samples, size_t n_samples,
                           size_t bytes_per_sample, void *rx_ctx) {

    int8_t *buff = (int8_t *)samples;
    sample_data_t *sd = malloc(sizeof(sample_data_t));

    sd->sample = malloc(n_samples * sizeof(size_t));
    memcpy(sd->sample, buff, n_samples);
    sd->size = n_samples;

    queue_append(&mag_line_queue, sd);
}

void *queue_processor() {
    while (!state.should_close && device_is_alive()) {
        pthread_mutex_lock(&lock);
        float *line = queue_pop(&mag_line_queue);
        if (line != NULL) {
            state.last_line = line;
        }
        free(line);
        if (queue_size(&mag_line_queue) > 25000) {
            queue_pop(&mag_line_queue);
        }
        pthread_mutex_unlock(&lock);
    }
}

void display_queue() {
    mvprintw(0, state.window.cols - 10, "[Q: %5d]",
             queue_size(&mag_line_queue));
    refresh();
}

void display_menu() {
    mvprintw(state.window.rows - 1, 0, "[q] Quit");
    refresh();
}

void display_terminal_size() {
    mvprintw(state.window.rows - 1, state.window.cols - 10, "[%3d, %3d]",
             state.window.cols, state.window.rows);
    refresh();
}

void keyboard_input(int ch) {
    if (ch == -1) {
        return;
    }
    switch (ch) {
    case 113: // q
    case 27:  // esc
        clear();
        mvprintw(0, 0, "Exiting...");
        refresh();
        state.should_close = true;
        break;
    default:
        break;
    }
    //    clear_row_block(1, 4, 10);
    //    mvprintw(1, 0, "Key: %d", ch);
    //    refresh();
}

void check_terminal_size() {
    getmaxyx(stdscr, state.window.rows, state.window.cols);
    if (state.window.rows != state.window.prev_rows ||
        state.window.cols != state.window.prev_cols) {
        clear();
        state.window.prev_rows = state.window.rows;
        state.window.prev_cols = state.window.cols;
    }
}

bool tick(clock_t time_begin, clock_t time_current) {
    return time_current >= (time_begin + 1 * CLOCKS_PER_SEC);
}

int main(int argc, char *argv[]) {
    state.should_close = false;

    signal(SIGABRT, handle_sigabrt);
    signal(SIGFPE, handle_sigfpe);
    signal(SIGILL, handle_sigill);
    signal(SIGINT, handle_sigint);
    signal(SIGSEGV, handle_sigsegv);
    signal(SIGTERM, handle_sigterm);

    queue_init(&mag_line_queue);
    fft_init(FFT_SIZE);
    if (!device_init()) {
        fprintf(stderr, "Could not open HackRF device\n");
        exit(-1);
    }

    // pthread_t queue_processing_thread;
    // pthread_create(&queue_processing_thread, NULL, queue_processor, NULL);

    dongle_init(&dongle);
    demod_init(&demod);
    output_init(&output);
    controller_init(&controller);

    demod.rate_in *= demod.post_downsample;
    if (!output.rate) {
        output.rate = demod.rate_out;
    }
    if (controller.freq_len > 1) {
        demod.terminate_on_squelch = 0;
    }
    ACTUAL_BUF_LENGTH = lcm_post[demod.post_downsample] * DEFAULT_BUF_LENGTH;
    verbose_device_search(dongle.dev_query, &dongle.dev);
    if (!dongle.dev) {
        fprintf(stderr, "Failed to open sdr device matching '%s'.\n",
                dongle.dev_query);
        exit(1);
    }
    verbose_setup_stream(dongle.dev, &dongle.stream, dongle.channel,
                         SOAPY_SDR_CS16);

    verbose_set_bandwidth(dongle.dev, dongle.bandwidth, dongle.channel);
    fprintf(stderr, "Supported bandwidth values in kHz:\n");
    size_t bw_count = 0;
    // TODO: well, this is deprecated by getBandwidthRange? SoapySDRRange

    fprintf(stderr, "\n");
    verbose_reset_buffer(dongle.dev);

    //    device_set_sample_rate(DEFAULT_SAMPLE_RATE);
    //    device_set_frequency(DEFAULT_FREQUENCY);
    //    device_set_lna_gain(DEFAULT_LNA_GAIN);
    //    device_set_vga_gain(DEFAULT_VGA_GAIN);
    //    device_rx(receive_callback_test);

    sleep(1);

    while (!state.should_close) {
        sample_data_t *sd = queue_pop(&mag_line_queue);
        if (sd != NULL) {
            for (int x = 0; x < sd->size - 1; x++) {
                iq_pair_t iq;
                iq.i = sd->sample[x];
                iq.q = sd->sample[x + 1];

                // dc_block_raw_filter()
            }
            free(sd->sample);
            free(sd);
            sd = NULL;
        }
    }

    //    device_destroy();
    int qs = queue_size(&mag_line_queue);
    printf("Queue size: %d\n", qs);
    return 0;

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, true);
    keypad(stdscr, true);
    curs_set(false);

    clock_t time_begin = clock();
    while (!state.should_close) {
        int ch = getch();
        keyboard_input(ch);

        clock_t time_current = clock();
        if (tick(time_begin, time_current)) {
            check_terminal_size();
            display_terminal_size();
            display_queue();
            display_menu();

            pthread_mutex_lock(&lock);
            for (int i = 0; i < FFT_SIZE; i++) {
                mvprintw(0, 0, "[%8.3f]", state.last_line[i]);
                refresh();
            }
            pthread_mutex_unlock(&lock);
            time_begin = time_current;
        }
    }

    endwin();
    // pthread_join(queue_processing_thread, NULL);
    pthread_mutex_destroy(&lock);
    device_destroy();
    fft_destroy();
    queue_destroy(&mag_line_queue);

    return 0;
}
