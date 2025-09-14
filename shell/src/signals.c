#include "shell.h"

void sigchld_handler(int sig) {
    child_completed = 1;
}

void setup_signal_handlers(void) {
    struct sigaction sa_int, sa_tstp;
    
    // SIGINT handler
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);
    
    // SIGTSTP handler
    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);
    
    // Ignore these signals
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

void sigtstp_handler(int sig) {
    if (fg_pgid > 0) {
        // Send SIGTSTP to foreground process group
        kill(-fg_pgid, SIGTSTP);
    }
}

void sigint_handler(int sig) {
    if (fg_pgid > 0) {
        // Send SIGINT to foreground process group
        kill(-fg_pgid, SIGINT);
    }
}