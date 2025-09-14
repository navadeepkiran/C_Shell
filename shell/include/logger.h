#ifndef LOGGER_H
#define LOGGER_H

// Logger function declarations
void add_to_log(const char *cmd, int write_to_file,char * home);
void save_log( char *home);
void load_log( char *home);
void handle_log(int n, char tokens[][1024], char *home, char *oldpwd);

#endif // LOGGER_H