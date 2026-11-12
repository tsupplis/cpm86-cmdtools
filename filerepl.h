#ifndef FILEREPL_H
#define FILEREPL_H

#include <stdio.h>

/* Reserve a new scratch file; never overwrite an existing file. */
#ifdef __STDC__
FILE *output_create(const char *path, const char *mode);
#else
FILE *output_create();
#endif

/* Install completed output, retaining recovery data on commit failure.
 * Returns zero on success, -1 on failure. */
#ifdef __STDC__
int output_install(const char *temporary, const char *name);
#else
int output_install();
#endif

#endif
