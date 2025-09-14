#ifndef PARSER_H
#define PARSER_H

// Parser function declarations
int tokenize(const char *input, char tokens[][1024]);
int is_name(const char *tok);
int parse_atomic(char tokens[][1024], int n, int idx);
int parse_cmd_group(char tokens[][1024], int n, int idx);
int parse_cmd(char tokens[][1024], int n, int idx);

#endif // PARSER_H