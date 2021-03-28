//
// Created by dbrent on 2/26/21.
// Andrew Ippoliti is the original author
//

#ifndef DBSDRT_ENDIAN_H
#define DBSDRT_ENDIAN_H

int is_big_endian();

int pack_int(char to_big_endian, int x);

void reverse_endianness(long long int size, void *value);

void to_big_endian(long long int size, void *value);

void to_little_endian(long long int size, void *value);

char big_endian_char(char value);

char little_endian_char(char value);

int big_endian_int(int value);

int little_endian_int(int value);

short int big_endian_short(short int value);

short int little_endian_short(short int value);

long int big_endian_long(long int value);

long int little_endian_long(long int value);

float big_endian_float(float value);

float little_endian_float(float value);

double big_endian_double(double value);

double little_endian_double(double value);

#endif // DBSDRT_ENDIAN_H
