//
// Created by dbrent on 2/25/21.
//

#ifndef DBSDRT_SIGNAL_HANDLER_H
#define DBSDRT_SIGNAL_HANDLER_H

void handle_sigabrt();

void handle_sigfpe();

void handle_sigill();

void handle_sigint();

void handle_sigsegv();

void handle_sigterm();

#endif // DBSDRT_SIGNAL_HANDLER_H
