//
// Created by dbrent on 2/25/21.
//

#include "signal_handler.h"
#include "globals.h"

#include <signal.h>
#include <stdio.h>

void handle_signal(int signal) {
    printf("\nSignal %d caught. Exiting.\n", signal);
    state.should_close = true;
}

void handle_sigabrt() { handle_signal(SIGABRT); }

void handle_sigint() { handle_signal(SIGINT); }

void handle_sigfpe() { handle_signal(SIGFPE); }

void handle_sigill() { handle_signal(SIGILL); }

void handle_sigsegv() { handle_signal(SIGSEGV); }

void handle_sigterm() { handle_signal(SIGTERM); }
