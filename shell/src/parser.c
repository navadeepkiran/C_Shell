#include "shell.h"

int tokenize(const char *input, char tokens[][1024]) {
    int count = 0;
    int i = 0, len = strlen(input);

    while (i < len) {
        while (i < len && isspace((unsigned char)input[i])) i++;
        if (i >= len) break;

        if (input[i] == '>' && i + 1 < len && input[i + 1] == '>') {
            strcpy(tokens[count++], ">>");
            i += 2;
            continue;
        }

        if (input[i] == '|' || input[i] == '&' || input[i] == ';' || input[i] == '<' || input[i] == '>') {
            tokens[count][0] = input[i];
            tokens[count][1] = '\0';
            count++;
            i++;
            continue;
        }

        int j = 0;
        while (i < len && !isspace((unsigned char)input[i]) &&
               input[i] != '|' && input[i] != '&' && input[i] != ';' &&
               input[i] != '<' && input[i] != '>') {
            tokens[count][j++] = input[i++];
        }
        tokens[count][j] = '\0';
        count++;
    }
    return count;
}

int is_name(const char *tok) {
    if (!tok) return 0;
    if (!strcmp(tok, "|") || !strcmp(tok, ";") || !strcmp(tok, "&") ||
        !strcmp(tok, "<") || !strcmp(tok, ">") || !strcmp(tok, ">>"))
        return 0;
    return 1;
}

int parse_atomic(char tokens[][1024], int n, int idx) {
    if (idx >= n || !is_name(tokens[idx])) return -1;
    idx++;

    while (idx < n) {
        if (!strcmp(tokens[idx], "<") || !strcmp(tokens[idx], ">") || !strcmp(tokens[idx], ">>")) {
            idx++;
            if (idx >= n || !is_name(tokens[idx])) return -1;
            idx++;
        } else if (is_name(tokens[idx])) {
            idx++;
        } else break;
    }
    return idx;
}

int parse_cmd_group(char tokens[][1024], int n, int idx) {
    idx = parse_atomic(tokens, n, idx);
    if (idx == -1) return -1;

    while (idx < n && !strcmp(tokens[idx], "|")) {
        idx++;
        idx = parse_atomic(tokens, n, idx);
        if (idx == -1) return -1;
    }
    return idx;
}


int parse_cmd(char tokens[][1024], int n, int idx) {
    idx = parse_cmd_group(tokens, n, idx);
    if (idx == -1) return -1;

    while (idx < n) {
        if (!strcmp(tokens[idx], ";")) {
            // semicolon must be followed by a cmd_group
            idx++;
            if (idx >= n) return -1;
            idx = parse_cmd_group(tokens, n, idx);
            if (idx == -1) return -1;
        } else if (!strcmp(tokens[idx], "&")) {
            // ampersand: two possibilities
            // 1) trailing ampersand -> accept and finish
            // 2) ampersand followed by another cmd_group -> parse it
            if (idx + 1 == n) {
                idx++;
                // trailing &, allowed idx++;
                break;
            } else {
                // not trailing: must have a cmd_group after it
                idx++;
                idx = parse_cmd_group(tokens, n, idx);
                if (idx == -1) return -1;
            }
        } else break;
    }
    return (idx == n) ? idx : -1;
}