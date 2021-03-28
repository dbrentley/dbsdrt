//
// Created by dbrent on 2/26/21.
//

#include "wave.h"
#include "endian.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

wave_header_t wave_create_header(int sample_rate, short int channels,
                                 short int bits_per_sample) {
    wave_header_t header;

    // RIFF WAVE header
    strcpy(header.chunk_id, "RIFF");
    strcpy(header.format, "WAVE");
    strcpy(header.sub_chunk_1_id, "fmt ");

    // format sub-chunk
    header.audio_format = 1; // pcm
    header.channels = channels;
    header.sample_rate = sample_rate;
    header.bits_per_sample = bits_per_sample;
    header.byte_rate =
            header.sample_rate * header.channels * header.bits_per_sample / 8;
    header.block_align =
            (short int)(header.channels * header.bits_per_sample / 8);

    // data sub-chunk
    strcpy(header.sub_chunk_2_id, "data");

    header.chunk_size = 4 + 8 + 16 + 8;
    header.sub_chunk_1_size = 16;
    header.sub_chunk_2_size = 0;

    return header;
}

wave_t wave_create(int sample_rate, short int channels,
                   short int bits_per_sample) {
    wave_t wave;
    wave.header = wave_create_header(sample_rate, channels, bits_per_sample);
    return wave;
}

void wave_set_duration(wave_t *wave, const float seconds) {
    long long int total_bytes =
            (long long int)(wave->header.byte_rate * seconds);
    wave->data = malloc(total_bytes);
    if (wave->data == NULL) {
        printf("Error allocating memory for wave data\n");
        exit(-1);
    }
    wave->index = 0;
    wave->size = total_bytes;
    wave->samples = (long long int)(wave->header.channels *
                                    wave->header.sample_rate * seconds);
    wave->header.chunk_size = 4 + 8 + 16 + 8 + total_bytes;
    wave->header.sub_chunk_2_size = (int)total_bytes;
}

void wave_add_sample(wave_t *wave, float *samples) {
    int i;
    short int sample_8_bit;
    int sample_16_bit;
    long int sample_32_bit;
    char *sample;

    switch (wave->header.bits_per_sample) {
    case 8:
        for (i = 0; i < wave->header.channels; i++) {
            sample_8_bit = (short int)(127 + 127.0 * samples[i]);
            to_little_endian(1, (void *)&sample_8_bit);
            sample = (char *)&sample_8_bit;
            (wave->data)[wave->index] = sample[0];
            wave->index += 1;
        }
        break;
    case 16:
        for (i = 0; i < wave->header.channels; i += 1) {
            sample_16_bit = (int)(32767 * samples[i]);
            to_little_endian(2, (void *)&sample_16_bit);
            sample = (char *)&sample_16_bit;
            wave->data[wave->index + 0] = sample[0];
            wave->data[wave->index + 1] = sample[1];
            wave->index += 2;
        }
        break;
    case 32:
        for (i = 0; i < wave->header.channels; i += 1) {
            sample_32_bit = (long int)((pow(2, 32 - 1) - 1) * samples[i]);
            to_little_endian(4, (void *)&sample_32_bit);
            sample = (char *)&sample_32_bit;
            wave->data[wave->index + 0] = sample[0];
            wave->data[wave->index + 1] = sample[1];
            wave->data[wave->index + 2] = sample[2];
            wave->data[wave->index + 3] = sample[3];
            wave->index += 4;
        }
        break;
    default:
        break;
    }
}

void wave_to_file(wave_t *wave, char *filename) {
    // http://blog.acipo.com/generating-wave-files-in-c/
    // convert to little endianness
    to_little_endian(sizeof(int), (void *)&(wave->header.chunk_size));
    to_little_endian(sizeof(int), (void *)&(wave->header.sub_chunk_1_size));
    to_little_endian(sizeof(short int), (void *)&(wave->header.audio_format));
    to_little_endian(sizeof(short int), (void *)&(wave->header.channels));
    to_little_endian(sizeof(int), (void *)&(wave->header.sample_rate));
    to_little_endian(sizeof(int), (void *)&(wave->header.byte_rate));
    to_little_endian(sizeof(short int), (void *)&(wave->header.block_align));
    to_little_endian(sizeof(short int),
                     (void *)&(wave->header.bits_per_sample));
    to_little_endian(sizeof(int), (void *)&(wave->header.sub_chunk_2_size));

    FILE *file = fopen(filename, "wb");
    fwrite(&(wave->header), sizeof(wave_header_t), 1, file);
    fwrite((void *)(wave->data), sizeof(char), wave->size, file);
    fclose(file);

    // convert back to system endianness
    to_little_endian(sizeof(int), (void *)&(wave->header.chunk_size));
    to_little_endian(sizeof(int), (void *)&(wave->header.sub_chunk_1_size));
    to_little_endian(sizeof(short int), (void *)&(wave->header.audio_format));
    to_little_endian(sizeof(short int), (void *)&(wave->header.channels));
    to_little_endian(sizeof(int), (void *)&(wave->header.sample_rate));
    to_little_endian(sizeof(int), (void *)&(wave->header.byte_rate));
    to_little_endian(sizeof(short int), (void *)&(wave->header.block_align));
    to_little_endian(sizeof(short int),
                     (void *)&(wave->header.bits_per_sample));
    to_little_endian(sizeof(int), (void *)&(wave->header.sub_chunk_2_size));
}

void wave_destroy(wave_t *wave) { free(wave->data); }