//
// Created by dbrent on 2/26/21.
// Andrew Ippoliti is the original author
//

#include "endian.h"

int is_big_endian() {
    int test = 1;
    char *p = (char *)&test;
    return p[0] == 0;
}

int pack_int(char to_big_endian, int x) {
    int my_int;
    char *packed = (char *)&my_int;
    char *ptr_to_x = (char *)&x;

    char needs_fix = !((to_big_endian && is_big_endian()) ||
                       !to_big_endian && !is_big_endian());

    if (needs_fix) {
        packed[0] = ptr_to_x[3];
        packed[1] = ptr_to_x[2];
        packed[2] = ptr_to_x[1];
        packed[3] = ptr_to_x[0];
    } else {
        packed[0] = ptr_to_x[0];
        packed[1] = ptr_to_x[1];
        packed[2] = ptr_to_x[2];
        packed[3] = ptr_to_x[3];
    }

    return my_int;
}

void reverse_endianness(long long int size, void *value) {
    int i;
    char result[32];
    for (i = 0; i < size; i++) {
        result[i] = ((char *)value)[size - i - 1];
    }
    for (i = 0; i < size; i++) {
        ((char *)value)[i] = result[i];
    }
}

void to_big_endian(long long int size, void *value) {
    if (!is_big_endian()) {
        reverse_endianness(size, value);
    }
}

void to_little_endian(long long int size, void *value) {
    if (is_big_endian()) {
        reverse_endianness(size, value);
    }
}

char big_endian_char(char value) {
    char copy = value;
    to_big_endian(sizeof(char), (void *)&copy);
    return copy;
}

char little_endian_char(char value) {
    char copy = value;
    to_big_endian(sizeof(char), (void *)&copy);
    return copy;
}

int big_endian_int(int value) {
    int copy = value;
    to_big_endian(sizeof(int), (void *)&copy);
    return copy;
}

int little_endian_int(int value) {
    int copy = value;
    to_big_endian(sizeof(int), (void *)&copy);
    return copy;
}

short int big_endian_short(short int value) {
    short int copy = value;
    to_big_endian(sizeof(short int), (void *)&copy);
    return copy;
}

short int little_endian_short(short int value) {
    short int copy = value;
    to_big_endian(sizeof(short int), (void *)&copy);
    return copy;
}

long int big_endian_long(long int value) {
    long int copy = value;
    to_big_endian(sizeof(long int), (void *)&copy);
    return copy;
}

long int little_endian_long(long int value) {
    long int copy = value;
    to_big_endian(sizeof(long int), (void *)&copy);
    return copy;
}

float big_endian_float(float value) {
    float copy = value;
    to_big_endian(sizeof(float), (void *)&copy);
    return copy;
}

float little_endian_float(float value) {
    float copy = value;
    to_big_endian(sizeof(float), (void *)&copy);
    return copy;
}

double big_endian_double(double value) {
    double copy = value;
    to_big_endian(sizeof(double), (void *)&copy);
    return copy;
}

double little_endian_double(double value) {
    double copy = value;
    to_big_endian(sizeof(double), (void *)&copy);
    return copy;
}
