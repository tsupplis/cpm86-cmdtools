/* BIN2ASM: convert binary bytes to NASM or MASM/ASM86 data declarations.
 * This code is licensed under the MIT license.
 */
#ifdef __STDC__
#include <string.h>
#include <stdlib.h>
#endif
#include "b2aconf.h"

#ifdef __STDC__
#if defined(__APPLE__) || defined(__GNUC__) || defined(__clang__)
#define BINARY_READ "rb"
#else
#define BINARY_READ "r"
#endif
#else
#define BINARY_READ "r"
#endif

/* Streaming skip also works on stdin, without a seekable file. */
#ifdef __STDC__
static int generate_asm(config_t *config, unsigned long *size) {
#else
static int generate_asm(config, size) config_t *config; unsigned long *size; {
#endif
    unsigned long left, offset = 0;
    unsigned char row[8];
    char ascii[9];
    int c, n, i, count;
    left = config->skip;
    while (left) {
        if (fgetc(config->in_file) == EOF) break;
        left--;
    }
    fprintf(config->out_file, "; ORIGIN(%04lx) SKIP(%04lx)\n", config->org, config->skip);
    for (;;) {
        n = 0;
        while (n < 8 && (!config->limited || offset + n < config->length)) {
            c = fgetc(config->in_file);
            if (c == EOF) break;
            row[n] = c;
            ascii[n] = c >= 0x20 && c <= 0x7E ? c : '.';
            n++;
        }
        if (!n) break;
        count = n;
        ascii[n] = 0;
        if (config->print_labels)
            fprintf(config->out_file, "label_%04lx:\n", config->org + config->skip + offset);
        fprintf(config->out_file, "    db  ");
        for (i = 0; i < n; i++) {
            if (i) fprintf(config->out_file, ", ");
            if (config->masm) fprintf(config->out_file, "0%02xh", (unsigned)row[i]);
            else fprintf(config->out_file, "0x%02x", (unsigned)row[i]);
        }
        for (i = n; i < 8; i++) fprintf(config->out_file, "      ");
        /* Avoid trailing blanks when the last byte is a space. */
        while (n && ascii[n-1] == ' ') ascii[--n] = 0;
        fprintf(config->out_file, "  ; %s\n", ascii);
        offset += count;
        *size = offset;
    }
    return 0;
}

#ifdef __STDC__
static int run(int argc, char **argv) {
#else
static int run(argc, argv) int argc; char **argv; {
#endif
    config_t config;
    unsigned long size = 0;
    int rc;
    rc = parse_config(&config, argc, argv);
    if (rc) { usage(); return rc == 1 ? 0 : 1; }
    if (config.in_file_name && !strcmp(config.in_file_name, "-")) config.in_file_name = NULL;
    if (config.out_file_name && !strcmp(config.out_file_name, "-")) config.out_file_name = NULL;
    if (config.in_file_name && config.out_file_name &&
        !strcmp(config.in_file_name, config.out_file_name)) {
        fprintf(stderr, "ERR: Input and output must have different names\n");
        return 1;
    }
    config.in_file = config.in_file_name ? fopen(config.in_file_name, BINARY_READ) : stdin;
    if (!config.in_file) {
        fprintf(stderr, "ERR: Cannot open input file '%s'\n", config.in_file_name);
        return 1;
    }
    config.out_file = config.out_file_name ? fopen(config.out_file_name, "w") : stdout;
    if (!config.out_file) {
        fprintf(stderr, "ERR: Cannot open output file '%s'\n", config.out_file_name);
        if (config.in_file_name) fclose(config.in_file);
        return 1;
    }
    rc = generate_asm(&config, &size);
    if (ferror(config.in_file)) {
        fprintf(stderr, "ERR: Cannot read input\n");
        rc = 1;
    }
    if (config.in_file_name) fclose(config.in_file);
    if (fflush(config.out_file) || ferror(config.out_file)) rc = 1;
    if (config.out_file_name && fclose(config.out_file)) rc = 1;
    if (rc) fprintf(stderr, "ERR: Conversion failed\n");
    else fprintf(stderr, "INF: FILE(%s) SYNTAX(%s) ORIGIN(%04lx) SKIP(%04lx) SIZE(%ld) OUTPUT(%s)\n",
                 config.in_file_name ? config.in_file_name : "stdin",
                 config.masm ? "MASM" : "NASM", config.org, config.skip, (long)size,
                 config.out_file_name ? config.out_file_name : "stdout");
    return rc;
}

#ifdef __STDC__
int main(int argc, char **argv) {
#else
int main(argc, argv) int argc; char **argv; {
#endif
    exit(run(argc, argv));
    return 0;
}
