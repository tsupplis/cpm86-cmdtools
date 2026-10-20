/* This code is licensed under the MIT license. */
#ifndef B2ACONF_H
#define B2ACONF_H
#include <stdio.h>

typedef struct {
    int masm, print_labels, limited;
    unsigned long org, skip, length;
    char *out_file_name, *in_file_name;
    FILE *out_file, *in_file;
} config_t;

#ifdef __STDC__
void usage(void);
int parse_config(config_t *config, int argc, char **argv);
#else
void usage();
int parse_config();
#endif
#endif
