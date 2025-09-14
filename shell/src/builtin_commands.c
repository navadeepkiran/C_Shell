#include "shell.h"

void shell_prompt(char *home) {
    char cwd[MAX_COMMAND_LENGTH];
    char hostname[MAX_COMMAND_LENGTH];
    char *username;
    username = getlogin();
    if (!username) {
        perror("get login failed");
    }
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("get hostname failed");
    }
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
    }
    if (strncmp(cwd, home, strlen(home)) == 0) {
        char final_cwd[MAX_COMMAND_LENGTH];
        int j = 0;
        for (int i = 0; i <= (int)strlen(cwd) - 1; i++) {
            if (i < (int)strlen(home)) continue;
            else {
                final_cwd[j++] = cwd[i];
            }
        }
        final_cwd[j] = '\0';
        fflush(stdout);
        printf("<%s@%s:~%s> ", username, hostname, final_cwd);
    } else {
        printf("<%s@%s:%s> ", username, hostname, cwd);
    }
}

void hop(char tokens[][1024], int n, char *home, char *oldpwd) {
    // If no argument 
    if (n == 1 && strcmp(tokens[0], "hop") == 0) {
        strcpy(tokens[1], "~");
        tokens[1][1] = '\0';
        n = 2;
    }

    for (int i = 1; i < n; i++) {
        char before[MAX_COMMAND_LENGTH];
        if (!getcwd(before, sizeof(before))) {
            perror("error in getting cwd\n");
            continue;
        }
        if (strcmp(tokens[i], "~") == 0) {
            if (chdir(home) == -1) {
                perror("hop\n");
            } else {
                strcpy(oldpwd, before);
                h = 1;
            }
        } else if (strcmp(tokens[i], ".") == 0) {
            continue;
        } else if (strcmp(tokens[i], "..") == 0) {
            if (chdir("..") == -1) {
                perror("hop\n");
            } else {
                strcpy(oldpwd, before);
                h = 1;
            }
        } else if (strcmp(tokens[i], "-") == 0) {
            if (oldpwd[0] == '\0') {
                continue;
            }
            if (chdir(oldpwd) == -1) {
                printf("No such directory!\n");
            } else {
                strcpy(oldpwd, before);
                h = 1;
            }
        } else {
            if (strcmp(tokens[i], ">") == 0 && strcmp(tokens[i], "<") == 0 && strcmp(tokens[i], ">>") == 0) {
                continue;
            }
            if (chdir(tokens[i]) == -1) {
                printf("No such directory!\n");
            } else {
                strcpy(oldpwd, before);
                h = 1;
            }
        }
    }
}

void reveal(char tokens[][1024], int n, char *home, char *oldpwd) {
    int idx = 1;
    int a = 0; 
    int l = 0; 
    char path[MAX_COMMAND_LENGTH]; 
    while (idx < n && tokens[idx][0] == '-' && tokens[idx][1]) {
        for (int j = 1; tokens[idx][j]; j++) {
            if (tokens[idx][j] == 'a') a = 1;
            else if (tokens[idx][j] == 'l') l = 1;
            else { 
                printf("reveal: Invalid Syntax!\n"); 
                return;
            }
        }
        idx++;
    }
    if (n - idx > 1) {
        printf("reveal: Invalid Syntax!\n");
        return;
    }
    if (idx == n) {
        getcwd(path, sizeof(path));
    } else {
        char *arg = tokens[idx];
        if (strcmp(arg, "~") == 0) {
            strcpy(path, home);
        } else if (strcmp(arg, ".") == 0) {
            getcwd(path, sizeof(path));
        } else if (strcmp(arg, "..") == 0) {
            strcpy(path, "..");
        } else if (strcmp(arg, "-") == 0) {
            if (oldpwd[0] == '\0' || h == 0) { 
                printf("No such directory!\n"); 
                return; 
            }
            strcpy(path, oldpwd);
        } else {
            strcpy(path, arg);
        }
    }
    DIR *dir = opendir(path);
    if (!dir) { 
        printf("No such directory!\n"); 
        return; 
    }
    char *names[4096];
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            if (!a) continue;  // skip hidden if no -a
        }
        names[count++] = strdup(entry->d_name);
        if (count >= 4096) break;
    }   
    closedir(dir);
    qsort(names, count, sizeof(char *), cmp_strptr);
    if (l) {
        for (int i = 0; i < count; i++) {
            printf("%s\n", names[i]);
            free(names[i]);
        }
    } else {
        for (int i = 0; i < count; i++) {
            printf("%s", names[i]);
            if (i + 1 < count) printf(" ");  // no trailing space
            free(names[i]);
        }
        printf("\n");
    }
}