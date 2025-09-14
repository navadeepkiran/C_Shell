#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

// Job control function declarations
void check_background_jobs(void);
void kill_jobs(void);
void activities(void);
void fg_command(char tokens[][1024], int n);
void bg_command(char tokens[][1024], int n);
void ping(char tokens[][1024]);
int find_job_by_number(int job_num);
int find_most_recent_job(void);
int compare_jobs(const void *a, const void *b);

#endif // JOBS_H