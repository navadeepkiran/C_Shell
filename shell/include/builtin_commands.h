#ifndef BUILTIN_COMMANDS_H
#define BUILTIN_COMMANDS_H

// Built-in command function declarations
void hop(char tokens[][1024], int n, char *home, char *oldpwd);
void reveal(char tokens[][1024], int n, char *home, char *oldpwd);
void shell_prompt(char *home);

#endif // BUILTIN_COMMANDS_H