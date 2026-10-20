/* This code is licensed under the MIT license. */
#ifdef __STDC__
#include <string.h>
#endif
#include "b2aconf.h"

/* Hex byte values, as in cmdmod; accept 0x and h notation as well. */
#ifdef __STDC__
static int number(char *s, unsigned long *value) {
#else
static int number(s, value) char *s; unsigned long *value; {
#endif
    unsigned long n = 0;
    int digit, count = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        if ((*s == 'h' || *s == 'H') && !s[1]) { s++; break; }
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') digit = *s - 'A' + 10;
        else return -1;
        if (n > (unsigned long)(0x7FFFFFFFL - digit) / 16) return -1;
        n = n * 16 + digit;
        count++;
        s++;
    }
    if (!count) return -1;
    *value = n;
    return 0;
}

#ifdef __STDC__
void usage(void) {
#else
void usage() {
#endif
    fprintf(stderr, "INF: bin2asm [-m] [-p] [-s skiphex] [-o originhex] [-l lengthhex]\n");
    fprintf(stderr, "               [-f file.asm] [file.bin [file.asm]]\n");
    fprintf(stderr, "     -m             MASM/ASM86 syntax (default: NASM)\n");
    fprintf(stderr, "     -p             emit address labels before each row\n");
    fprintf(stderr, "     -s skiphex     skip input bytes (default: 0)\n");
    fprintf(stderr, "     -o originhex   label origin, added to skip (default: 0)\n");
    fprintf(stderr, "     -l lengthhex   maximum bytes to convert (default: to EOF)\n");
    fprintf(stderr, "     -f file.asm    output file (default: stdout)\n");
    fprintf(stderr, "     -h             display this help\n");
    fprintf(stderr, "     numbers are hex; 0x and h notation are also accepted\n");
    fprintf(stderr, "     omit input or use '-' for stdin; '--' ends options\n");
}

#ifdef __STDC__
int parse_config(config_t *config, int argc, char **argv) {
#else
int parse_config(config, argc, argv) config_t *config; int argc; char **argv; {
#endif
    int i, ch, positional = 0, options = 1;
    char *arg, *value;
    memset(config, 0, sizeof(config_t));
    if (argc == 1) {
        fprintf(stderr, "ERR: Invalid command line\n");
        return -1;
    }
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        if (options && strcmp(arg, "--") == 0) { options = 0; continue; }
        if (options && arg[0] == '-' && arg[1]) {
            if (arg[2]) {
                fprintf(stderr, "ERR: Unknown option '%s'\n", arg);
                return -1;
            }
            ch = arg[1];
            if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
            if (ch == 'h') return 1;
            if (ch == 'm') { config->masm = 1; continue; }
            if (ch == 'p') { config->print_labels = 1; continue; }
            if (ch != 's' && ch != 'o' && ch != 'l' && ch != 'f') {
                fprintf(stderr, "ERR: Unknown option '%s'\n", arg);
                return -1;
            }
            if (++i == argc) {
                fprintf(stderr, "ERR: Option '%s' requires a value\n", arg);
                return -1;
            }
            value = argv[i];
            if (ch == 'f') {
                if (config->out_file_name) {
                    fprintf(stderr, "ERR: Output file specified more than once\n");
                    return -1;
                }
                config->out_file_name = value;
            } else if (number(value, ch == 's' ? &config->skip :
                              ch == 'o' ? &config->org : &config->length)) {
                fprintf(stderr, "ERR: -%c value must be a hex number up to 7FFFFFFFh\n", ch);
                return -1;
            } else if (ch == 'l') config->limited = 1;
        } else {
            if (positional == 0) config->in_file_name = arg;
            else if (positional == 1 && !config->out_file_name)
                config->out_file_name = arg;
            else {
                fprintf(stderr, "ERR: Too many file arguments\n");
                return -1;
            }
            positional++;
        }
    }
    return 0;
}
