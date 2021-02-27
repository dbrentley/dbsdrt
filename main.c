#include "device.h"
#include "fft.h"
#include "globals.h"
#include "queue.h"
#include "signal_handler.h"

#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_SAMPLE_RATE 20000000
#define DEFAULT_FREQUENCY 106120000 // 860721500 //
#define DEFAULT_LNA_GAIN 24
#define DEFAULT_VGA_GAIN 24
#define FFT_SIZE 8192 // max BYTES_PER_TRANSFER

queue_t mag_line_queue;
state_t state;

void receive_callback(void *samples, size_t n_samples,
                      size_t bytes_per_sample) {
    int8_t *buff = (int8_t *)samples;
    size_t ffts = n_samples / FFT_SIZE;

    // TODO store in a buffer for fft size larger than n samples

    for (size_t i = 0; i < ffts; i++) {
        float *line = fft(buff + i * FFT_SIZE * bytes_per_sample);
        queue_append(&mag_line_queue, line);
    }
}

void clear_row_block(int y, int x, int count) {
    move(y, x);
    refresh();
    for (int i = 0; i < count; i++) {
        delch();
    }
    move(y, x);
    refresh();
}

void *queue_processor() {
    while (!state.should_close && device_is_alive()) {
        float *line = queue_pop(&mag_line_queue);
        if (line != NULL) {
            state.last_line = line;
        }
        free(line);
        if (queue_size(&mag_line_queue) > 25000) {
            queue_pop(&mag_line_queue);
        }
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
    device_set_sample_rate(DEFAULT_SAMPLE_RATE);
    device_set_frequency(DEFAULT_FREQUENCY);
    device_set_lna_gain(DEFAULT_LNA_GAIN);
    device_set_vga_gain(DEFAULT_VGA_GAIN);
    device_rx(receive_callback);

    pthread_t queue_processing_thread;
    pthread_create(&queue_processing_thread, NULL, queue_processor, NULL);

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

            for (int i = 0; i < FFT_SIZE; i++) {
                mvprintw(0, 0, "[%8.3f]", state.last_line[i]);
                refresh();
            }
            time_begin = time_current;
        }
    }

    endwin();
    pthread_join(queue_processing_thread, NULL);
    device_destroy();
    fft_destroy();
    queue_destroy(&mag_line_queue);

    return 0;
}
