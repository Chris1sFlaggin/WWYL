#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE  

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h> 
#include <string.h>
#include <stdio.h>

void fatal_error(const char *fmt, ...);
void *safe_zalloc(size_t size);
void errExit(const char *msg);
char *getRandomWord(unsigned int random_seed);

#endif