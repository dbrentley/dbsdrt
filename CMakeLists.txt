cmake_minimum_required(VERSION 3.14)
project(dbsdrt C)

set(CMAKE_C_STANDARD 11)

add_executable(dbsdrt main.c device.c device.h fft.c fft.h queue.c queue.h
        globals.h signal_handler.c signal_handler.h wave.c wave.h endian.c
        endian.h demod.c demod.h convenience.c convenience.h)
target_link_libraries(dbsdrt ncurses hackrf pthread fftw3 m SoapySDR)
