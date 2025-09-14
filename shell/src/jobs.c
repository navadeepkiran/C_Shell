#include "shell.h"

int compare_jobs(const void *a, const void *b) {
    const BackgroundJob *ja = (const BackgroundJob *)a;
    const BackgroundJob *jb = (const BackgroundJob *)b;
    return strcmp(ja->command, jb->command);
}

void kill_jobs(void) {
    for (int i = 0; i < job_count; i++) {
        kill(jobs[i].pid, SIGKILL);
    }
}

void ping(char tokens[][1024]) {
    int pid = atoi(tokens[1]);
    int signal_num = atoi(tokens[2]);
    int actual_sig = signal_num % 32;
    if (kill(pid, actual_sig) == -1) {
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            perror("kill failed");
        }
    } else {
        printf("Sent signal %d to process with pid %d\n", signal_num, pid);
    }
}

void activities(void) {
    BackgroundJob sorted[MAX_JOBS];
    memcpy(sorted, jobs, job_count * sizeof(BackgroundJob));
    qsort(sorted, job_count, sizeof(BackgroundJob), compare_jobs);
    for (int i = 0; i < job_count; i++) {
        printf("[%d] : %s - %s\n", sorted[i].pid, sorted[i].command, sorted[i].state);
    }
}


void check_background_jobs(void) {
    int status;
    pid_t pid;
    
    // Check all jobs
    for (int i = 0; i < job_count; i++) {
        pid = waitpid(jobs[i].pid, &status, WNOHANG);
        
        if (pid > 0) {
            // Job has completed
            if (WIFEXITED(status)) {
                int es = WEXITSTATUS(status);
                if (es == 0) {
                    printf("%s with pid %d exited normally\n", 
                           jobs[i].command, jobs[i].pid);
                } else {
                    printf("%s with pid %d exited abnormally\n", 
                           jobs[i].command, jobs[i].pid);
                }
            } else if (WIFSIGNALED(status)) {
                printf("%s with pid %d was terminated by signal %d\n", 
                       jobs[i].command, jobs[i].pid, WTERMSIG(status));
            }
            
            // Remove job from list
            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j+1];
            }
            job_count--;
            i--; // Adjust index after removal
        }
    }
}

int find_job_by_number(int job_num) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_num == job_num) {
            return i;
        }
    }
    return -1;
}

int find_most_recent_job(void) {
    if (job_count == 0) return -1;
    
    int max_num = -1;
    int max_index = -1;
    
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].job_num > max_num) {
            max_num = jobs[i].job_num;
            max_index = i;
        }
    }
    
    return max_index;
}

void fg_command(char tokens[][1024], int n) {
    int job_index = -1;
    
    if (n == 1) {
        // No job number provided, use most recent job
        job_index = find_most_recent_job();
        if (job_index == -1) {
            printf("No jobs available\n");
            return;
        }
    } else if (n == 2) {
        // Job number provided
        int job_num = atoi(tokens[1]);
        job_index = find_job_by_number(job_num);
        if (job_index == -1) {
            printf("No such job\n");
            return;
        }
    } else {
        printf("fg: Invalid Syntax\n");
        return;
    }
    
    BackgroundJob *job = &jobs[job_index];
    printf("%s\n", job->command);
    
    // If job is stopped, send SIGCONT to resume it
    if (strcmp(job->state, "Stopped") == 0) {
        kill(job->pid, SIGCONT);
        strcpy(job->state, "Running");
    }
    
    // Set as foreground process
    fg_pid = job->pid;
    fg_pgid = job->pid;
    
    // Give terminal control to the job
    tcsetpgrp(STDIN_FILENO, fg_pgid);
    
    // Wait for the job to complete or stop again
    int status;
    waitpid(job->pid, &status, WUNTRACED);
    
    // Restore terminal control to shell
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    
    if (WIFSTOPPED(status)) {
        // Job was stopped again
        strcpy(job->state, "Stopped");
        printf("\n[%d] Stopped %s\n", job->job_num, job->command);
    } else {
        // Job completed - remove from job list
        for (int j = job_index; j < job_count - 1; j++) {
            jobs[j] = jobs[j+1];
        }
        job_count--;
    }
    
    fg_pid = -1;
    fg_pgid = -1;
}

void bg_command(char tokens[][1024], int n) {
    int job_index = -1;
    
    if (n == 1) {
        // No job number provided, use most recent job
        job_index = find_most_recent_job();
        if (job_index == -1) {
            printf("No jobs available\n");
            return;
        }
    } else if (n == 2) {
        // Job number provided
        int job_num = atoi(tokens[1]);
        job_index = find_job_by_number(job_num);
        if (job_index == -1) {
            printf("No such job\n");
            return;
        }
    } else {
        printf("bg: Invalid Syntax\n");
        return;
    }
    
    BackgroundJob *job = &jobs[job_index];
    
    if (strcmp(job->state, "Running") == 0) {
        printf("Job already running\n");
        return;
    }
    
    if (strcmp(job->state, "Stopped") == 0) {
        // Resume the stopped job
        kill(job->pid, SIGCONT);
        strcpy(job->state, "Running");
        printf("[%d] %s &\n", job->job_num, job->command);
    } else {
        printf("Job is not stopped\n");
    }
}