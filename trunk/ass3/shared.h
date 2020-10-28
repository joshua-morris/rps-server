#include <stdio.h>
#include <stdbool.h>

#ifndef SHARED_H
#define SHARED_H

#define INITIAL_BUFFER_SIZE 80

char* read_line(FILE*);
bool check_tag(char*, char*);

#endif
