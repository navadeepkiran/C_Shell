#include "shell.h"

void add_to_log(const char *cmd, int write_to_file,char * home) {
    // log command
    if (strncmp(cmd, "log", 3) == 0) return;

    // skip if same as last stored
    if (log_count > 0) {
        int last_idx = (log_start + log_count - 1) % MAX_LOG_ENTRIES;
        if (strcmp(log_buf[last_idx], cmd) == 0) return;
    }

    if (log_count < MAX_LOG_ENTRIES) {
        strcpy(log_buf[(log_start + log_count) % MAX_LOG_ENTRIES], cmd);
        log_count++;
    } else {
        // overwrite oldest
        strcpy(log_buf[log_start], cmd);
        log_start = (log_start + 1) % MAX_LOG_ENTRIES;
    }

    if (write_to_file) save_log(home);
}

void save_log(char * home) {
    char filename[100];
    sprintf(filename,"%s/log.txt",home);
    FILE *fp = fopen(filename, "w");
    if (!fp) return;
    for (int i = 0; i < log_count; i++) {
        int idx = (log_start + i) % MAX_LOG_ENTRIES;
        fprintf(fp, "%s\n", log_buf[idx]);
    }
    fclose(fp);
}

void load_log(char *home) {
    char filename[100];
    sprintf(filename,"%s/log.txt",home);
    FILE *fp = fopen(filename, "r");
    if (!fp) return;
    char line[MAX_COMMAND_LENGTH];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';  // remove newline
        if (strlen(line) > 0) {
            add_to_log(line, 0,home);  // 0 = don't re-save
        }
    }
    fclose(fp);
}

void handle_log(int n, char tokens[][1024], char *home, char *oldpwd) {
    char filename[100];
    sprintf(filename,"%s/log.txt",home);
    if (log_count == 0) {
        log_start = 0;
        log_count = 0;
        
        FILE *fp = fopen(filename, "r");
        if (fp) {
            while (log_count < 16 && fgets(log_buf[log_count], MAX_COMMAND_LENGTH, fp) != NULL) {
                int len = strlen(log_buf[log_count]);
                if (len > 0 && log_buf[log_count][len - 1] == '\n') {
                    log_buf[log_count][len - 1] = '\0';
                }
                log_count++;
            }
            fclose(fp);
        }
    }
    if (n == 1) {
        for (int i = 0; i < log_count; i++) {
            int idx = (log_start + i) % MAX_LOG_ENTRIES;
            printf("%s\n", log_buf[idx]);
        }
    } else if (strcmp(tokens[1], "purge") == 0) {
        log_count = 0;
        log_start = 0;
        FILE *fp = fopen(filename, "w");
        if (fp) fclose(fp);
    } else if (strcmp(tokens[1], "execute") == 0 && n >= 3) {
        int index = into_int(tokens[2]);
        if (index <= 0 || index > log_count) {
            printf("Invalid index!\n");
            return;
        }
        
        int idx = (log_start + log_count - index) % MAX_LOG_ENTRIES;
        char *cmd_to_run = log_buf[idx];
        semicolon(cmd_to_run, home, oldpwd);
    } else {
        printf("log: Invalid Syntax!\n");
    }
}