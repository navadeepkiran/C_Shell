#include "shell.h"

void semicolon(char *command, char *home, char *oldpwd) {
    command[strcspn(command, "\n")] = '\0';
    char tokens[MAX_TOKENS][MAX_COMMAND_LENGTH];
    int n = tokenize(command, tokens);
    int res = parse_cmd(tokens, n, 0);
    if (res == -1) {
        printf("Invalid Syntax!\n");
    } else {
        int p = 0;
        for (int i = 0; i <= n - 1; i++)
            if (strcmp(tokens[i], "log") == 0)
                p = 1;
        if (p == 0)
            add_to_log(command, 1,home);
            
        if (strcmp(tokens[0], "ping") == 0) {
            if (n == 3) {
                ping(tokens);
                return;
            } else {
                printf("Invalid Syntax\n");
                return;
            }
        }
        if (strcmp(tokens[0], "fg") == 0) {
            if (n > 2) {
                printf("Invalid Syntax\n");
                return;
            }
            fg_command(tokens, n);
            return;
        }
        if (strcmp(tokens[0], "bg") == 0) {
            if (n > 2) {
                printf("Invalid Syntax\n");
                return;
            }
            bg_command(tokens, n);
            return;
        }

        for (int i = 0; i <= n - 1; i++) {
            if (strcmp(tokens[i], ";") == 0 || strcmp(tokens[i], "&") == 0) {
                semi_cmd(tokens, n, home, oldpwd);
                return;
            }
        }
        apply_cmd(tokens, n, home, oldpwd);
        return;
    }
}

void semi_cmd(char tokens[][1024], int n, char *home, char *oldpwd) {
    int start = 0;
    for (int i = 0; i <= n - 1; i++) {
        background = 0;
        if (strcmp(tokens[i], ";") == 0) {
            if (i > start) {
                char new_tokens[MAX_TOKENS][MAX_COMMAND_LENGTH];
                int count = 0;
                for (int j = start; j < i; j++) {
                    strcpy(new_tokens[count++], tokens[j]);
                }
                apply_cmd(new_tokens, count, home, oldpwd);
            }
            start = i + 1;
        } else if (strcmp(tokens[i], "&") == 0) {
            background = 1;
            if (i > start) {
                char new_tokens[MAX_TOKENS][MAX_COMMAND_LENGTH];
                int count = 0;
                for (int j = start; j < i; j++) {
                    strcpy(new_tokens[count++], tokens[j]);
                }
                apply_cmd(new_tokens, count, home, oldpwd);
                background = 0;
            }
            start = i + 1;
        } else if (i == n - 1) {
            if (i >= start) {
                char new_tokens[MAX_TOKENS][MAX_COMMAND_LENGTH];
                int count = 0;
                for (int j = start; j <= i; j++) {
                    strcpy(new_tokens[count++], tokens[j]);
                }
                apply_cmd(new_tokens, count, home, oldpwd);
            }
            start = i + 1;
        }
    }
}

void apply_cmd(char tokens[][1024], int n, char *home, char *oldpwd) {
    int pipe = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            pipe = 1;
            break;
        }
    }
    
    if (strcmp(tokens[0], "hop") == 0 && pipe == 0) {
        hop(tokens, n, home, oldpwd);
    } else if (strcmp(tokens[0], "reveal") == 0 && pipe == 0) {
        pid_t pid = fork();
        if (pid == 0) {
            execute_normal_segment(tokens, 0, n - 1, 0, 1, home, oldpwd);
            exit(0);
        } else {
            waitpid(pid, NULL, 0);
        }
    } else if (strcmp(tokens[0], "activities") == 0 && pipe == 0) {
        // Check for redirection in the tokens
        int has_redir = 0;
        for (int i = 1; i < n; i++) {
            if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0 || strcmp(tokens[i], ">>") == 0) {
                has_redir = 1;
                break;
            }
        }
        if (has_redir) {
            pid_t pid = fork();
            if (pid == 0) {
                execute_normal_segment(tokens, 0, n - 1, 0, 1, home, oldpwd);
                exit(0);
            } else {
                waitpid(pid, NULL, 0);
            }
        } else {
            activities(); // Direct call without redirection
        }
    } else if (strcmp(tokens[0], "log") == 0 && pipe == 0) {
        int has_redir = 0;
        for (int i = 1; i < n; i++) {
            if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0 || strcmp(tokens[i], ">>") == 0) {
                has_redir = 1;
                break;
            }
        }
        if (has_redir) {
            pid_t pid = fork();
            if (pid == 0) {
                execute_normal_segment(tokens, 0, n - 1, 0, 1, home, oldpwd);
                exit(0);
            } else {
                waitpid(pid, NULL, 0);
            }
        } else {
            handle_log(n, tokens, home, oldpwd);
        }
    } else if (strcmp(tokens[0], "fg") == 0 && pipe == 0) {
        fg_command(tokens, n);
    } else if (strcmp(tokens[0], "bg") == 0 && pipe == 0) {
        bg_command(tokens, n);
    } else {
        if (pipe) {
            execute_pipeline(tokens, n, home, oldpwd);
        } else {
            char last_cmd[MAX_COMMAND_LENGTH] = "";
                for (int j = 0; j < n; j++) {
                    strcat(last_cmd, tokens[j]);
                    if(j!=n-1)
                    strcat(last_cmd, " ");
                }
            last_cmd[sizeof(last_cmd) - 1] = '\0';
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork failed");
                return;
            } else if (pid == 0) {
                // Child process
                setpgid(0, 0); // Create new process group
                
                // Reset signal handlers to default
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_DFL);
                signal(SIGQUIT, SIG_DFL);
                
                // If background, redirect stdin to /dev/null
                if (background) {
                    int null_fd = open("/dev/null", O_RDONLY);
                    if (null_fd != -1) {
                        dup2(null_fd, STDIN_FILENO);
                        close(null_fd);
                    }
                }
                
                execute_normal_segment(tokens, 0, n - 1, 0, 1, home, oldpwd);
                exit(1);
            } else {
                // Parent process
                if (!background) {
                    // Foreground process
                    setpgid(pid, pid);
                    fg_pid = pid;
                    fg_pgid = pid;
                    
                    // Give terminal control to the foreground process
                    tcsetpgrp(STDIN_FILENO, fg_pgid);
                    
                    int status;
                    waitpid(pid, &status, WUNTRACED);
                    
                    // Take back terminal control
                    tcsetpgrp(STDIN_FILENO, shell_pgid);
                    
                    if (WIFSTOPPED(status)) {
                        // Add to jobs list as stopped
                        jobs[job_count].job_num = next_job_num++;
                        jobs[job_count].pid = pid;
                        strncpy(jobs[job_count].command, last_cmd, sizeof(jobs[job_count].command) - 1);
                        jobs[job_count].command[sizeof(jobs[job_count].command) - 1] = '\0';
                        strcpy(jobs[job_count].state, "Stopped");
                        printf("\n[%d] Stopped %s\n", jobs[job_count].job_num, jobs[job_count].command);
                        job_count++;
                    }
                    fg_pid = -1;
                    fg_pgid = -1;
                } else {
                    // Background process
                    setpgid(pid, pid);
                    jobs[job_count].job_num = next_job_num++;
                    jobs[job_count].pid = pid;
                    strncpy(jobs[job_count].command, last_cmd, sizeof(jobs[job_count].command) - 1);
                    jobs[job_count].command[sizeof(jobs[job_count].command) - 1] = '\0';
                    strcpy(jobs[job_count].state, "Running");
                    printf("[%d] %d\n", jobs[job_count].job_num, pid);
                    job_count++;
                }
            }
        }
    }
}

void execute_normal_segment(char tokens[][1024], int start, int end, int in_fd, int out_fd, char *home, char *oldpwd) {
    char *argv[MAX_TOKENS];
    char *file1 = NULL;
    char *file2 = NULL;
    char *file3 = NULL;
    int argc = 0;

    // parse <,>,>>
    for (int i = start; i <= end; i++) {
        if (strcmp(tokens[i], "<") == 0) {
            if (i < end) {
                file1 = tokens[i + 1];
                i++;
                int fd = open(file1, O_RDONLY);
                if (fd < 0) {
                    fprintf(stderr, "No such file or directory\n");
                    exit(1);
                }
                close(fd);
            } else {
                perror("Invalid Syntax in <");
                exit(1);
            }
        } else if (strcmp(tokens[i], ">") == 0) {

            if (i < end) {
                file2 = tokens[i + 1];
                i++;
               int fd = open(file2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
               if (fd < 0) {
                   fprintf(stderr, "Unable to create file for writing\n");
                   exit(1);
                }
                close(fd);
            } else {
                perror("Invalid Syntax >");
                exit(1);
            }
        } else if (strcmp(tokens[i], ">>") == 0) {
            if (i < end) {
                file3 = tokens[i + 1];
                i++;
                int fd = open(file3, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) {
                    fprintf(stderr, "Unable to create file for writing\n");
                    exit(1);
                }
                close(fd);
            } else {
                perror("Invalid Syntax >> ");
                exit(1);
            }
        } else {
            argv[argc++] = tokens[i];
        }
    }
    argv[argc] = NULL;

    // input redirection
    if (file1) {
        int fd = open(file1, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "No such file or directory\n");
            exit(1);
        }
        dup2(fd, 0);
        close(fd);
    } else if (in_fd != 0) {
        dup2(in_fd, 0);
    }
    
    // output redirection
    if (file2) {
        int fd = open(file2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "Unable to create file for writing\n");
            exit(1);
        }
        dup2(fd, 1);
        close(fd);
    } else if (file3) {
        int fd = open(file3, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            fprintf(stderr, "Unable to create file for writing\n");
            exit(1);
        }
        dup2(fd, 1);
        close(fd);
    } else if (out_fd != 1) {
        dup2(out_fd, 1);
    }
    
    if (strcmp(argv[0], "activities") == 0) {
        activities();
        exit(0);
    } else if (strcmp(argv[0], "reveal") == 0) {
        reveal(tokens + start, argc, home, oldpwd);
        exit(0);
    } else if (strcmp(argv[0], "hop") == 0) {
        hop(tokens + start, argc, home, oldpwd);
        exit(0);
    } else if (strcmp(argv[0], "log") == 0) {
        handle_log(argc, tokens + start, home, oldpwd);
        exit(0);
    } else if (strcmp(argv[0], "fg") == 0) {
        fg_command(tokens + start, argc);
    } else if (strcmp(argv[0], "bg") == 0) {
        bg_command(tokens + start, argc);
    }
    
    if (execvp(argv[0], argv) < 0)
        fprintf(stderr, "Command not found!\n");
    exit(1);
}

void execute_pipeline(char tokens[][1024], int n, char *home, char *oldpwd) {
    int cmds = 1;
    
    for (int i = 0; i < n; i++)
        if (strcmp(tokens[i], "|") == 0) cmds++;
    if (cmds == 0) return;
    
    int pipes[cmds - 1][2];
    for (int i = 0; i <= cmds - 2; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return;
        }
    }
    
    int start = 0;
    pid_t first_pid = -1;
    
    for (int cmd = 0; cmd < cmds; cmd++) {
        int end = start;
        while (end <= n - 1 && strcmp(tokens[end], "|") != 0) end++;
        
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            start = end + 1;
            continue;
        }
        
        if (pid == 0) {
            if (first_pid == -1) {
                setpgid(0, 0); // First process becomes group leader
            } else {
                setpgid(0, first_pid); // Join the first process's group
            }
            // Set up pipe connections
            int in_fd = 0;
            if (cmd == 0) in_fd = 0;
            else in_fd = pipes[cmd - 1][0];
            
            int out_fd = 0;
            if (cmd == cmds - 1) out_fd = 1;
            else out_fd = pipes[cmd][1];
            
            if (in_fd != 0) {
                dup2(in_fd, 0);
            }
            if (out_fd != 1) {
                dup2(out_fd, 1);
            }
            
            // Close all pipes
            for (int i = 0; i < cmds - 1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
            
            // Execute this segment
            execute_normal_segment(tokens, start, end - 1, in_fd, out_fd, home, oldpwd);
            exit(1);
        } else {
            if (first_pid == -1) {
                first_pid = pid;
                setpgid(pid, first_pid);
            } else {
                setpgid(pid, first_pid);
            }
        }
        
        start = end + 1; // move to next segment
    }
    
    // Parent: close all pipes
    for (int i = 0; i < cmds - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    if (!background) {
        // Foreground pipeline
        fg_pid = first_pid;
        fg_pgid = first_pid;
        tcsetpgrp(STDIN_FILENO, fg_pgid);
        
        // Wait for all children
        for (int i = 0; i < cmds; i++) {
            int status;
            waitpid(-first_pid, &status, WUNTRACED);
            
            if (WIFSTOPPED(status)) {
                // Add to jobs list as stopped
                jobs[job_count].job_num = next_job_num++;
                jobs[job_count].pid = first_pid;
                
                // Build command string for the pipeline
                char pipeline_cmd[MAX_COMMAND_LENGTH] = "";
                for (int j = 0; j < n; j++) {
                    strcat(pipeline_cmd, tokens[j]);
                    if(j!=n-1)
                    strcat(pipeline_cmd, " ");
                }
                
                strncpy(jobs[job_count].command, pipeline_cmd, sizeof(jobs[job_count].command) - 1);
                jobs[job_count].command[sizeof(jobs[job_count].command) - 1] = '\0';
                strcpy(jobs[job_count].state, "Stopped");
                printf("\n[%d] Stopped %s\n", jobs[job_count].job_num, jobs[job_count].command);
                job_count++;
                break;
            }
        }
        
        // Restore terminal control
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        fg_pid = -1;
        fg_pgid = -1;
    } else {
        // Background pipeline - add to jobs list
        jobs[job_count].job_num = next_job_num++;
        jobs[job_count].pid = first_pid;
        
        // Build command string for the pipeline
        char pipeline_cmd[MAX_COMMAND_LENGTH] = "";
        for (int j = 0; j < n; j++) {
            strcat(pipeline_cmd, tokens[j]);
            if(j!=n-1)
            strcat(pipeline_cmd, " ");
        }
        
        strncpy(jobs[job_count].command, pipeline_cmd, sizeof(jobs[job_count].command) - 1);
        jobs[job_count].command[sizeof(jobs[job_count].command) - 1] = '\0';
        strcpy(jobs[job_count].state, "Running");
        printf("[%d] %d\n", jobs[job_count].job_num, first_pid);
        job_count++;
    }
}