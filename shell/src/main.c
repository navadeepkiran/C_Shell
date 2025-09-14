#include "shell.h"

// Global variable definitions
char log_buf[MAX_LOG_ENTRIES][MAX_COMMAND_LENGTH];
int log_count = 0;
int log_start = 0;
int h = 0;
int background = 0;
pid_t fg_pid = -1;
pid_t fg_pgid = -1;
pid_t shell_pgid = -1;
BackgroundJob jobs[MAX_JOBS];
int job_count = 0;
int next_job_num = 1;
char last_cmd[MAX_COMMAND_LENGTH];
volatile sig_atomic_t child_completed = 0;

int main() {
    char home[MAX_COMMAND_LENGTH];
    if (getcwd(home, sizeof(home)) == NULL) {
        perror("home failed");
        exit(1);
    }
    
    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    
    setup_signal_handlers();
    
    char oldpwd[MAX_COMMAND_LENGTH];
    oldpwd[0] = '\0';
    
    printf("# I am currently running bash\n");
    
    while (1) {
        check_background_jobs();
        
        shell_prompt(home);
        
        char command[MAX_COMMAND_LENGTH];
        if (fgets(command, sizeof(command), stdin) != NULL) {
            semicolon(command, home, oldpwd);
        } else {
            if (feof(stdin)) {
                /* Ctrl-D (EOF) -> kill all child processes, print logout, exit 0 */
                kill_jobs();
                printf("logout\n");
                exit(0);
            } else if (errno == EINTR) {
                // fgets was interrupted by a signal, continue
                clearerr(stdin);
                printf("\n");
                continue;
            } else {
                perror("fgets failed");
                continue;
            }
        }
    }
    
    return 0;
}