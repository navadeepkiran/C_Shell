#include "shell.h"

int cmp_strptr(const void *a, const void *b) {
    const char *const *sa = (const char *const *)a;
    const char *const *sb = (const char *const *)b;
    return strcmp(*sa, *sb);
}

int into_int(char *str) {
    if (strlen(str) == 1) return str[0] - '0';
    else {
        int i = str[0] - '0';
        int j = str[1] - '0';
        return i * 10 + j;
    }
}