#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>
#include <sys/types.h>

// Signal handler function declarations
void setup_signal_handlers(void);
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void sigchld_handler(int sig);

#endif // SIGNALS_H