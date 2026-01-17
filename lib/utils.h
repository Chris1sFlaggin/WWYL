#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h> 
#include <string.h>
#include <stdio.h>

void fatal_error(const char *fmt, ...);
void *safe_zalloc(size_t size);
void errExit(const char *msg);

#endif