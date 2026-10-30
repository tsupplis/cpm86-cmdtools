/*    Convert CP/M / Intel HEX to a contiguous binary image.
 *
 *    Data records: 00.
 *    A zero-length type 00 record or a type 01 record ends the file;
 *    CP/M padding after it is ignored. Record 03 gives a start address,
 *    not image content. All addresses are 16-bit.
 *
 *    Validate before opening the output. An 8K address bitmap and a second
 *    input pass avoid a 64K image allocation on 16-bit small-model builds.
 */

#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#endif
#include <errno.h>
#include <stdio.h>

#ifdef __STDC__
#if defined(__APPLE__) || defined(__gnu_linux__) || defined(__GNUC__) ||       \
    defined(__clang__)
#include <unistd.h>
#define BINARY_READ "rb"
#define BINARY_WRITE "wb"
#define ATOMIC_RENAME
#else
#define BINARY_READ "r"
#define BINARY_WRITE "w"
#endif
#else
#define SEEK_SET 0
#define BINARY_READ "r"
#define BINARY_WRITE "w"
#endif

static unsigned char seen[8192];
static unsigned char record[260];
static long line_number, record_line;
static long lowest, highest;

#ifdef __STDC__
int hex_digit(int c) {
#else
int hex_digit(c)
int c;
{
#endif
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

#ifdef __STDC__
int white(int c) {
#else
int white(c)
int c;
{
#endif
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

#ifdef __STDC__
int bad_record(char *name, char *message) {
#else
int bad_record(name, message)
char *name, *message;
{
#endif
    fprintf(stderr, "ERR: %s:%ld: %s\n", name, record_line, message);
    return -1;
}

/* Return 1 for a valid record, 0 for physical EOF, -1 for an error. */
#ifdef __STDC__
int read_record(FILE *input, char *name) {
#else
int read_record(input, name)
FILE *input;
char *name;
{
#endif
    int c, digit, half, count, sum;
    for (;;) {
        c = fgetc(input);
        if (c == EOF) {
            if (ferror(input)) {
                fprintf(stderr, "ERR: Can't read input '%s' (%d)\n", name,
                        errno);
                return -1;
            }
            return 0;
        }
        if (c == '\n') {
            line_number++;
            continue;
        }
        if (!white(c))
            break;
    }
    record_line = line_number;
    if (c != ':')
        return bad_record(name, "Not a hex record");
    count = sum = 0;
    half = -1;
    while ((c = fgetc(input)) != EOF && c != '\n') {
        /* Ignore CR in CRLF input. */
        if (c == '\r')
            continue;
        if (white(c)) {
            if (half >= 0)
                return bad_record(name, "Not hexadecimal");
            continue;
        }
        digit = hex_digit(c);
        if (digit < 0)
            return bad_record(name, "Not hexadecimal");
        if (half < 0)
            half = digit;
        else {
            if (count == sizeof(record))
                return bad_record(name, "Record is too long");
            record[count] = half * 16 + digit;
            sum += record[count++];
            sum &= 255;
            half = -1;
        }
    }
    if (ferror(input)) {
        fprintf(stderr, "ERR: Can't read input '%s' (%d)\n", name, errno);
        return -1;
    }
    if (c == '\n')
        line_number++;
    if (half >= 0)
        return bad_record(name, "Not hexadecimal");
    if (count < 5 || count != record[0] + 5)
        return bad_record(name, "Record length does not match");
    if (sum)
        return bad_record(name, "Bad checksum");
    return 1;
}

#ifdef __STDC__
int validate(FILE *input, char *name) {
#else
int validate(input, name)
FILE *input;
char *name;
{
#endif
    int result, type, i, bit, ended;
    long address, end, pos;
    memset(seen, 0, sizeof(seen));
    lowest = 65536L;
    highest = 0;
    line_number = 1;
    ended = 0;
    while ((result = read_record(input, name)) > 0) {
        type = record[3];
        if (type == 1 || (type == 0 && !record[0])) {
            ended = 1;
            break;
        }
        if (type == 3)
            continue;
        if (type != 0)
            return bad_record(name, "Unsupported record type");
        address = (long)record[1] * 256 + record[2];
        end = address + record[0];
        if (end > 65536L)
            return bad_record(name, "Record exceeds the 16-bit address space");
        for (i = 0; i < record[0]; i++) {
            pos = address + i;
            bit = 1 << (int)(pos & 7);
            if (seen[(unsigned int)(pos >> 3)] & bit)
                return bad_record(name, "Address is covered twice");
            seen[(unsigned int)(pos >> 3)] |= bit;
        }
        if (record[0]) {
            if (address < lowest)
                lowest = address;
            if (end > highest)
                highest = end;
        }
    }
    if (result < 0)
        return -1;
    if (!ended) {
        fprintf(stderr, "ERR: %s: No end of file record\n", name);
        return -1;
    }
    if (lowest == 65536L) {
        fprintf(stderr, "ERR: %s: No data records\n", name);
        return -1;
    }
    for (pos = lowest; pos < highest; pos++) {
        if (!(seen[(unsigned int)(pos >> 3)] & (1 << (int)(pos & 7)))) {
            fprintf(stderr, "ERR: %s: Hole in image at %04lx\n", name, pos);
            return -1;
        }
    }
    return 0;
}

/* CP/M upper-cases names; reject aliases differing only in case too. */
#ifdef __STDC__
int same_name(char *a, char *b) {
#else
int same_name(a, b)
char *a, *b;
{
#endif
    int x, y;
    do {
        x = *a++;
        y = *b++;
        if (x >= 'a' && x <= 'z')
            x -= 'a' - 'A';
        if (y >= 'a' && y <= 'z')
            y -= 'a' - 'A';
        if (x != y)
            return 0;
    } while (x);
    return 1;
}

#ifdef __STDC__
void usage(void) {
#else
usage() {
#endif
    fprintf(stderr, "ERR: Invalid command line\n");
    fprintf(stderr, "INF: hex2bin file.hex file.bin\n");
    fprintf(stderr,
            "     - converts CP/M / Intel HEX to a flat binary image\n");
    fprintf(
        stderr,
        "     - strips the origin; rejects holes, overlaps and bad records\n");
}

#ifdef __STDC__
int main_alt(int argc, char **argv) {
#else
int main_alt(argc, argv)
int argc;
char **argv;
{
#endif
    FILE *input, *output;
    static char temporary[300];
    char *p, *dot;
    int result, failed;
    long address;
    input = output = NULL;
    if (argc != 3) {
        usage();
        return 1;
    }
    if (same_name(argv[1], argv[2])) {
        fprintf(stderr, "ERR: Input and output must be different files\n");
        return 1;
    }
    if (strlen(argv[2]) > sizeof(temporary) - 5) {
        fprintf(stderr, "ERR: Output file name is too long\n");
        return 1;
    }
    strcpy(temporary, argv[2]);
    dot = NULL;
    for (p = temporary; *p; p++) {
        if (*p == '.')
            dot = p;
        else if (*p == '/' || *p == '\\' || *p == ':')
            dot = NULL;
    }
    strcpy(dot ? dot : p, ".$$$");
    if (same_name(temporary, argv[1]) || same_name(temporary, argv[2])) {
        fprintf(stderr, "ERR: Can't derive a temporary name from '%s'\n",
                argv[2]);
        return 1;
    }
    input = fopen(argv[1], BINARY_READ);
    if (!input) {
        fprintf(stderr, "ERR: Can't open input '%s' (%d)\n", argv[1], errno);
        return 1;
    }
    if (validate(input, argv[1]) < 0)
        goto fail;
    if (fseek(input, 0L, SEEK_SET)) {
        fprintf(stderr, "ERR: Can't rewind input '%s' (%d)\n", argv[1], errno);
        goto fail;
    }
    fprintf(stderr, "INF: FILE(%s) ORIGIN(%04lx) SIZE(%ld)\n", argv[1], lowest,
            highest - lowest);
    output = fopen(temporary, BINARY_WRITE);
    if (!output) {
        fprintf(stderr, "ERR: Can't open output '%s' (%d)\n", temporary, errno);
        goto fail;
    }
    failed = 0;
    line_number = 1;
    while ((result = read_record(input, argv[1])) > 0) {
        if (record[3] == 1 || (record[3] == 0 && !record[0]))
            break;
        if (record[3] == 3 || !record[0])
            continue;
        address = (long)record[1] * 256 + record[2] - lowest;
        if (fseek(output, address, SEEK_SET) ||
            fwrite(record + 4, 1, record[0], output) != record[0]) {
            fprintf(stderr, "ERR: Can't write output '%s' (%d)\n", temporary,
                    errno);
            failed = 1;
            break;
        }
    }
    if (result <= 0 || failed)
        goto fail;
    if (fclose(output)) {
        output = NULL;
        unlink(temporary);
        fprintf(stderr, "ERR: Can't close output '%s' (%d)\n", temporary,
                errno);
        goto fail;
    }
    output = NULL;
    fclose(input);
    input = NULL;
#ifndef ATOMIC_RENAME
    unlink(argv[2]);
#endif
    if (rename(temporary, argv[2])) {
        fprintf(stderr, "ERR: Can't replace output '%s' (%d); result in '%s'\n",
                argv[2], errno, temporary);
        return 1;
    }
    fprintf(stderr, "INF: OUTPUT(%s) SIZE(%ld)\n", argv[2], highest - lowest);
    return 0;
fail:
    if (output) {
        fclose(output);
        unlink(temporary);
    }
    if (input)
        fclose(input);
    return 1;
}

#ifdef __STDC__
int main(int argc, char **argv) {
#else
int main(argc, argv)
int argc;
char **argv;
{
#endif
    exit(main_alt(argc, argv));
    return 0;
}
