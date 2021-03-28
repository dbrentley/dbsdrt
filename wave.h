//
// Created by dbrent on 2/26/21.
//

#ifndef DBSDRT_WAVE_H
#define DBSDRT_WAVE_H

typedef struct {
    // riff wave header
    char chunk_id[4];
    int chunk_size;
    char format[4];

    // format sub-chunk
    char sub_chunk_1_id[4];
    int sub_chunk_1_size;
    short int audio_format;
    short int channels;
    int sample_rate;
    int byte_rate;
    short int block_align;
    short int bits_per_sample;

    // data sub-chunk
    char sub_chunk_2_id[4];
    int sub_chunk_2_size;
} wave_header_t;

typedef struct {
    wave_header_t header;
    char *data;
    long long int index;
    long long int size;
    long long int samples;
} wave_t;

wave_header_t wave_create_header(int sample_rate, short int channels,
                                 short int bits_per_sample);

wave_t wave_create(int sample_rate, short int channels,
                   short int bits_per_sample);

void wave_set_duration(wave_t *wave, float seconds);

void wave_destroy(wave_t *wave);

void wave_add_sample(wave_t *wave, float *samples);

void wave_to_file(wave_t *wave, char *filename);

#endif // DBSDRT_WAVE_H
