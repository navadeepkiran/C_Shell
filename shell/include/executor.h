#ifndef EXECUTOR_H
#define EXECUTOR_H

// Execution function declarations
void semicolon(char *command, char *home, char *oldpwd);
void semi_cmd(char tokens[][1024], int n, char *home, char *oldpwd);
void apply_cmd(char tokens[][1024], int n, char *home, char *oldpwd);
void execute_normal_segment(char tokens[][1024], int start, int end, int in_fd, int out_fd, char *home, char *oldpwd);
void execute_pipeline(char tokens[][1024], int n, char *home, char *oldpwd);

#endif // EXECUTOR_H