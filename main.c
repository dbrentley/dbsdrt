#include "device.h"
#include "fft.h"
#include "globals.h"
#include "queue.h"
#include "signal_handler.h"

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

void *queue_processor() {
    while (!state.should_close && device_is_alive()) {
        float *line = queue_pop(&mag_line_queue);
        if (line != NULL) {
            state.last_line = line;
        }
        free(line);
        // this shouldn't happen but just in case
        if (queue_size(&mag_line_queue) > 25000) {
            queue_pop(&mag_line_queue);
        }
    }
}

int main(int argc, char *argv[]) {

    signal(SIGABRT, handle_sigabrt);
    signal(SIGFPE, handle_sigfpe);
    signal(SIGILL, handle_sigill);
    signal(SIGINT, handle_sigint);
    signal(SIGSEGV, handle_sigsegv);
    signal(SIGTERM, handle_sigterm);

    state.should_close = 0;
    clock_t time_begin = clock();

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

    while (!state.should_close) {
        time_t timer = clock() - time_begin;
        long msec = timer * 1000 / CLOCKS_PER_SEC;
        if (msec > 1000) {
            printf("Queue size: %zu\n", queue_size(&mag_line_queue));
            time_begin = clock();
        }
    }

    pthread_join(queue_processing_thread, NULL);
    device_destroy();
    fft_destroy();
    queue_destroy(&mag_line_queue);

    return 0;
}
