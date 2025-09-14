#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <termios.h>

#define MAX_JOBS 100
#define MAX_LOG_ENTRIES 15
#define MAX_COMMAND_LENGTH 1024
#define MAX_TOKENS 1024

// Structure for background jobs
typedef struct {
    int job_num;
    pid_t pid;
    char command[MAX_COMMAND_LENGTH];
    char state[20]; // "Running", "Stopped"
} BackgroundJob;

// Global variables
extern char log_buf[MAX_LOG_ENTRIES][MAX_COMMAND_LENGTH];
extern int log_count;
extern int log_start;
extern int h;
extern int background;
extern pid_t fg_pid;
extern pid_t fg_pgid;
extern pid_t shell_pgid;
extern BackgroundJob jobs[MAX_JOBS];
extern int job_count;
extern int next_job_num;
extern char last_cmd[MAX_COMMAND_LENGTH];
extern volatile sig_atomic_t child_completed;

// Function declarations from various modules
#include "signals.h"
#include "jobs.h"
#include "builtin_commands.h"
#include "parser.h"
#include "executor.h"
#include "logger.h"
#include "utils.h"

#endif // SHELL_H